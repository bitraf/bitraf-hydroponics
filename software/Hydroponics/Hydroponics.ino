
/*
  14CORE NodeMCU i2C 16x2 LCD SCREEN
  Test COde.........................
*/

#include <Wire.h>  // This library is already built in to the Arduino IDE
#include <ArduinoOTA.h>
#include "LiquidCrystal_I2C.h" //This library you can add via Include Library > Manage Library > 
#include "RotaryEncoder.h"
#include "Ota.h"

LiquidCrystal_I2C lcd(0x3F, 16, 2);

const char *wifi_ssid = ;
const char *wifi_password = ;

const int PIN_ENC1 = D5;
const int PIN_ENC2 = D6;
const int PIN_BTN = D7;
using decoder_t = RotaryEncoderDecoder<PIN_ENC1, PIN_ENC2, PIN_BTN, uint16_t>;

void rotaryChange(duration_type duration_type, uint16_t click_length);
void rotaryMove(bool dir);

static decoder_t rotary(rotaryMove, rotaryChange);

void intrEncChange1() {
  rotary.intr1();
}

void intrEncChange2() {
  rotary.intr2();
}
void setup()
{
  Serial.begin(115200);
  Serial.println();
  Serial.println("Bitraf hydroponics");
  
  lcd.init();   // initializing the LCD
  lcd.backlight(); // Enable or Turn On the backlight
  lcd.setCursor(0, 0);
  //lcd.print("A"); // Start Print text to Line 1
  lcd.print("_-~/\., (\")"); // Start Print text to Line 1
  lcd.setCursor(0, 1);
  lcd.print("Line 2----------"); // Start Print Test to Line 2

  rotary.setup();
  attachInterrupt(digitalPinToInterrupt(PIN_ENC1), intrEncChange1, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC2), intrEncChange2, CHANGE);

  setupOta();
  WiFi.begin(wifi_ssid, wifi_password);
}

void loop() {
  ArduinoOTA.handle();

  rotary.loop();
}

void rotaryMove(bool dir) {
  static int count = 0;
  Serial.println(dir ? ">>>" : "<<<");
  lcd.setCursor(0, 0);
  lcd.print(dir ? ">>>" : "<<<");
  lcd.setCursor(0, 1);
  count += dir ? 1 : -1;
  lcd.print(count);
}

void rotaryChange(duration_type duration_type, uint16_t duration) {

  static const char* pressText[] = {
    "really quick press!",
    "short press",
    "normal press",
    "long press",
    "very looooooooong press"
  };

  Serial.print("released selecting: ");
  Serial.print(rotary.currentValue(), DEC);
  Serial.print(" (after ");
  Serial.print(duration);
  Serial.print(" ms = ");
  Serial.print(pressText[static_cast<int>(duration_type)]);
  Serial.println(")");
}

