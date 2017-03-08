
#include <Wire.h>  // This library is already built in to the Arduino IDE
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include "LiquidCrystal_I2C.h" //This library you can add via Include Library > Manage Library >
#include "RotaryEncoder.h"
#include <PubSubClient.h>
#include "Ota.h"
#include <TimeLib.h>
#include <NtpClientLib.h>
#include "tx433_Nexa.h" // Nexa headers
#include "wifipassword.h" //This is not commited to git so provide your own like example above.


// Nexa ID
String tx_nexa = "1010101010101010101010101010101010101010101010101010";
String ch_nexa="1010";

// RF transmit
const int RF_DATA = D3;
Tx433_Nexa Nexa(RF_DATA, tx_nexa, ch_nexa);

// Light timers
int light_time_on[2] = {8, 0};
int light_time_off[2] = {0, 0};


// WiFi Variables
WiFiClient wifi_client;
const char *wifi_ssid = WIFI_SSID;
const char *wifi_password = WIFI_PWD;

// MQTT client
const char *mqtt_address = MQTT_ADDRESS;
const int mqtt_port = MQTT_PORT;
const char *mqtt_client_id = MQTT_CLIENT_ID;

PubSubClient mqtt_client(wifi_client);

// MQTT topics
const char *mqtt_light_on = MQTT_TOPIC_LIGHT_ON;
const char *mqtt_light_off = MQTT_TOPIC_LIGHT_OFF;

// LCD setup
LiquidCrystal_I2C lcd(0x3F, 16, 2);

static bool LIGHT_STATUS = false;

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

bool timeSynced() {
  //Consider returning an enum with a status for "sync", "out of sync" or "no time set"
  if (NTP.getLastNTPSync() == 0){
    Serial.println("Time not synced");
    return false;
    }
  else if (now() - NTP.getLastNTPSync() > NTP.getInterval() * 3){
    Serial.print("time since last sync: ");
    Serial.println(now() - NTP.getLastNTPSync());
    return true;
    }
  else return true;
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

  while (!WiFi.isConnected()){
    Serial.println("Waiting for WiFi ");
    delay(500);
  }

  NTP.begin("pool.ntp.org", 1, true);
  NTP.setInterval(60000);

  mqtt_client.setServer(mqtt_address, mqtt_port);
  mqtt_client.setCallback(onMqttMsg);
  mqtt_client.connect(mqtt_client_id);
  Serial.print("MQTT Connecton state: ");
  Serial.println(mqtt_client.state());
  mqtt_client.subscribe(mqtt_light_on);
  mqtt_client.subscribe(mqtt_light_off);


}

void loop() {
  ArduinoOTA.handle();
  rotary.loop();
  mqtt_client.loop();

  if (timeSynced())
    // Make sure light is on/off as planned. Accurate within the hour
    checkLight();

  static int i = 0;
  static int last = 0;
  if ((millis() - last) > 5100) {

    last = millis();
    Serial.print(i); Serial.print(" ");
    Serial.print(NTP.getTimeDateString()); Serial.print(". ");
    Serial.print("WiFi is ");
    Serial.print(WiFi.isConnected() ? "connected" : "not connected"); Serial.print(". ");
    Serial.print("Uptime: ");
    Serial.print(NTP.getUptimeString()); Serial.print(" since ");
    Serial.println(NTP.getTimeDateString(NTP.getFirstSync()).c_str());
    Serial.print("Time now: ");
    digitalClockDisplay();

    i++;
  }
}

void onMqttMsg(char* _topic, byte* payload, unsigned int length) {
  String topic(_topic);
  String value;

  value.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) {
    value += static_cast<char>(payload[i]);
  }

  if (topic.endsWith("/light-on")) {
    if (parseTime(value, light_time_on[0], light_time_on[1])) {
      Serial.print("Setting new light on time: ");
      Serial.println(value);
    }
  } else if (topic.endsWith("/light-off")) {
    if (parseTime(value, light_time_off[0], light_time_off[1])) {
      Serial.print("Setting new light off time: ");
      Serial.println(value);
    }
  }
}

bool parseTime(const String &value, int &hour, int &minute){
  uint colon = value.indexOf(":");

  if (colon == -1) {
    Serial.println("Failed parsing time. No colon found");
    return false;
  }

  String hour_sub = value.substring(0, colon);
  String minute_sub = value.substring(colon + 1);

  if (hour_sub.length() >= 1 && minute_sub.length() >= 1){
    int hour_int = hour_sub.toInt();
    int minute_int = minute_sub.toInt();

    if (hour_int < 0 || hour_int > 23){
      Serial.print("parseTime Failed! Hour out of range: ");
      Serial.println(hour_int);
      return false;
    }

    if (minute_int < 0 || minute_int > 59) {
      Serial.print("parseTime Failed! Minute out of range: ");
      Serial.println(minute_int);
      return false;
    }

    // Success!
    hour = hour_int;
    minute = minute_int;
    return true;
  }

  Serial.println("Failed parsing time. Digits missing before or after colon");
  return false;
}

void digitalClockDisplay(){
  // digital clock display of the time
  Serial.print(hour());
  printDigits(minute());
  printDigits(second());
  Serial.print(" ");
  Serial.print(day());
  Serial.print(".");
  Serial.print(month());
  Serial.print(".");
  Serial.print(year());
  Serial.println();
}

void printDigits(int digits){
  // utility for digital clock display: prints preceding colon and leading 0
  Serial.print(":");
  if(digits < 10)
    Serial.print('0');
  Serial.print(digits);
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

// Light On
void lightOn(){
  if (!LIGHT_STATUS) {
    Serial.println("Light on, Good morning!");
    Nexa.Device_On(0);
    LIGHT_STATUS = true;
  }
}

// Light timer
void lightOff(){
  if (LIGHT_STATUS) {
    Serial.println("Light Off, Good nights!");
    Nexa.Device_Off(0);
    LIGHT_STATUS = false;
  }
}

void checkLight() {
  if (light_time_off[0] < light_time_on[0]) {
      if (hour() >= light_time_on[0] && minute() >= light_time_on[1])
        lightOn();
      else
        lightOff();
  }
  else {
      if (hour() >= light_time_on[0] && hour() <= light_time_off[0]) {
        if (minute() >= light_time_on[1] && minute() < light_time_off[1])
          lightOn();

        else
          lightOff();
      }
      else
        lightOff();
    }
}
