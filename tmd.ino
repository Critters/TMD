#include <HX711_ADC.h>
#include <EEPROM.h>

const float PORTERFILTERWEIGHT = 459;
const int TRIGGERWEIGHT = 250;

const int HX711_dout = 4; //mcu > HX711 dout pin
const int HX711_sck = 5; //mcu > HX711 sck pin
const int BUTTONPIN = 3;
const int calVal_eepromAdress = 0;
const int zero_eepromAdress = 1;
float calibrationValue;

int lastLoadCellValue = 0.0;
int lastPeakLoad = 0.0;
int zeroCount = 0;

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128   
#define SCREEN_HEIGHT 32   
#define OLED_RESET     -1  
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool btnPressed;
long int btnPressedTime;

HX711_ADC LoadCell(HX711_dout, HX711_sck);

void setup() {
  Serial.begin(57600); delay(10);
  Serial.print("Starting...");

  pinMode(BUTTONPIN, INPUT_PULLUP);

  // SCREEN
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); 
  }
  display.setRotation(2);

  display.clearDisplay();
  display.setTextSize(2); 
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(22, 10);
  display.println(F("TMD v0.2"));
  display.display();

  // LOAD CELL
  LoadCell.begin();
  LoadCell.start(500, true);
  EEPROM.get(calVal_eepromAdress, calibrationValue);
  LoadCell.setCalFactor(calibrationValue);

  EEPROM.get(zero_eepromAdress, zeroCount);
  Serial.println("done");
}

void loop() {
  // Button
  if (!btnPressed && !digitalRead(BUTTONPIN)) {
    // DOWN
    btnPressed = true;
    btnPressedTime = millis();
  }
  if (btnPressed && digitalRead(BUTTONPIN)) {
    // UP
    btnPressed = false;
    if ((millis() - btnPressedTime) < 1000) {
      // Short press - reset zero      
      zero();
    }else if((millis() - btnPressedTime) > 5000) {
      // Long press - calibrate
      calibrate();
    }
  }

  // Load Cell
  if (LoadCell.update()) {
    lastLoadCellValue = int(LoadCell.getData());
    //Serial.println(lastLoadCellValue);
    if (lastLoadCellValue < 0) lastLoadCellValue = 0;
  }

  if (lastLoadCellValue >= TRIGGERWEIGHT) {
    modeTamp();
  }else{
    modeIdle();
  }
  
}

void modeIdle(){
  display.clearDisplay();
  if(lastPeakLoad > 0) {
    display.fillRoundRect(1, 1, 126, 30, 15, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
    display.setCursor(10, 10);
    display.print(String(formatWeight(lastPeakLoad)) + String(" lbs"));
  }

  display.display();
}

double formatWeight(int weight) {
  return weight/453.59;
}

void modeTamp(){
  display.clearDisplay();
  
  display.drawRoundRect(4, 24, 118, 26, 2, SSD1306_WHITE);

  int w = (118 / 11339.8) * lastLoadCellValue;
  if(w < 0) w = 0;
  if(w > 118) w = 118;
  display.fillRoundRect(4, 24, w, 26, 2, SSD1306_WHITE);

  display.setTextColor(SSD1306_WHITE);

  int displayValue = int(lastLoadCellValue);
  if(displayValue > lastPeakLoad) lastPeakLoad = displayValue;

  display.setCursor(22, 7);
  display.println(formatWeight(displayValue), 2);
  display.display();
}

void zero(){
  if(zeroCount%20 == 0){
    TMD();
  }
  display.clearDisplay();
  display.setTextSize(2); 
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(22, 9);
  display.println(F("ZEROING"));
  display.display();
  delay(500);
  LoadCell.start(4000, true);

  zeroCount += 1;
  EEPROM.put(zero_eepromAdress, zeroCount);
  Serial.println(zeroCount);

  lastPeakLoad = 0.0;
}

void TMD(){
  display.clearDisplay();
  display.setTextSize(2); 
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(40, 9);
  display.println(F("TAMP"));
  display.display();
  delay(500);

  display.clearDisplay();
  display.setTextSize(2); 
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(40+8, 9);
  display.println(F("ME"));
  display.display();
  delay(500);

  for(int i = 0; i<80; i++){
    display.clearDisplay();
    display.setTextSize(2); 
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(39-8+random(2),8+random(2));
    display.println(F("DADDY!"));
    display.display();
  }
}

void calibrate(){
  // ZERO
  display.clearDisplay();
  display.setTextSize(1); 
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Remove everything and click."));
  display.display();
  bool _resume = false;
  bool _clicked = false;
  delay(1000);
  while(_resume == false) {
    LoadCell.update();
    if (!digitalRead(BUTTONPIN) && !_clicked) {
      _clicked = true;
      LoadCell.tareNoDelay();
    }
    if (_clicked && LoadCell.getTareStatus() == true) {
      _resume = true;
    }
  }
  
  // CALIBRATE WITH KNOWN WEIGHT
  display.clearDisplay();
  display.setTextSize(2); 
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("Add EMPTY portafilter and click."));
  display.display();
  _resume = false;
  _clicked = false;
  delay(1000);
  while(_resume == false) {
    LoadCell.update();
    if (!digitalRead(BUTTONPIN) && !_clicked) {
      _clicked = true;
      _resume = true;
    }
  }

  display.clearDisplay();
  display.setTextSize(2); 
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(22, 8);
  display.println(F("WAIT..."));
  display.display();

  // ACTUAL CALIBRATION
  LoadCell.refreshDataSet();
  float newCalibrationValue = LoadCell.getNewCalibration(PORTERFILTERWEIGHT);
  Serial.println(newCalibrationValue);
  EEPROM.put(calVal_eepromAdress, newCalibrationValue);

  display.clearDisplay();
  display.setTextSize(2); 
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(8, 8);
  display.println(F("CALIBRATED"));
  display.display();
  delay(2000);
}
