
#include <Wire.h>  // This library is already built in to the Arduino IDE
#include <ArduinoOTA.h>
#include <WiFiClient.h>
#include "LiquidCrystal_I2C.h" //This library you can add via Include Library > Manage Library >
#include "RotaryEncoder.h"
#include <PubSubClient.h>
#include "Ota.h"
#include <TimeLib.h>
#include <TimeAlarms.h>
#include <NtpClientLib.h>
#include "tx433_Nexa.h" // Nexa headers
#include "wifipassword.h" //This is not commited to git so provide your own like example above.
#include <ArduinoJson.h>
#include <FS.h>
// Nexa ID
String tx_nexa = "1010101010101010101010101010101010101010101010101010";
String ch_nexa="1010";

// Settings variables
unsigned long water_fill_time;
int light_time_on[2];
int light_time_off[2];
int air_time_on[2];
int air_time_off[2];
int water_cycles[4][2];
const char *wifi_ssid;
const char *wifi_password;

// RF transmit
const int RF_DATA = D3;
Tx433_Nexa Nexa(RF_DATA, tx_nexa, ch_nexa);

// MOSFET 3
const int MOSFET_3 = D4;
// Water PIN
const int WATER_PUMP_PIN = D6;

// Track last water fill
unsigned long last_fill_start;

// Track if water is or should be on
bool water_on = false;

// Track if light is on or should be
bool light_on = false;

// Track if air pump is on or should be
bool air_on = false;

// Check water interval in minutes
const uint CHECK_WATER_INTERVAL = 1;

// Air pump PIN
const int AIR_PUMP_PIN = D5;

// WiFi Variables
WiFiClient wifi_client;

// MQTT client
const char *mqtt_address = MQTT_ADDRESS;
const int mqtt_port = MQTT_PORT;
const char *mqtt_client_id = MQTT_CLIENT_ID;

PubSubClient mqtt_client(wifi_client);

// MQTT topics
const char *mqtt_light_on = MQTT_TOPIC_LIGHT_ON;
const char *mqtt_light_off = MQTT_TOPIC_LIGHT_OFF;
const char *mqtt_air_on = MQTT_TOPIC_AIR_ON;
const char *mqtt_air_off = MQTT_TOPIC_AIR_OFF;
const char *mqtt_watercycle = MQTT_TOPIC_WATERCYCLE;
const char *mqtt_water_fill_time = MQTT_TOPIC_WATER_FILL_TIME;
const char *mqtt_status = MQTT_TOPIC_STATUS;

// LCD setup
LiquidCrystal_I2C lcd(0x3F, 16, 2);

const int PIN_ENC1 = D8;
const int PIN_ENC2 = D9;
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

// MQTT
void startMQTT(){
  mqtt_client.setServer(mqtt_address, mqtt_port);
  mqtt_client.setCallback(onMqttMsg);
  mqtt_client.connect(mqtt_client_id);
  Serial.print("MQTT Connecton state: ");
  Serial.println(mqtt_client.state());
  mqtt_client.subscribe(mqtt_light_on);
  mqtt_client.loop();
  mqtt_client.subscribe(mqtt_light_off);
  mqtt_client.loop();
  mqtt_client.subscribe(mqtt_air_on);
  mqtt_client.loop();
  mqtt_client.subscribe(mqtt_air_off);
  mqtt_client.loop();
  mqtt_client.subscribe(mqtt_watercycle);
  mqtt_client.loop();
  mqtt_client.subscribe(mqtt_water_fill_time);
  mqtt_client.loop();
  mqtt_client.subscribe(mqtt_status);
  mqtt_client.loop();
}

// Start NTP only after IP network is connected
void onSTAGotIP(WiFiEventStationModeGotIP ipInfo) {
	Serial.printf("Got IP: %s\r\n", ipInfo.ip.toString().c_str());
  Serial.println("Starting NTP");
	NTP.begin("pool.ntp.org", 1, true);
	NTP.setInterval(5, 63);
}

// Manage network disconnection
void onSTADisconnected(WiFiEventStationModeDisconnected event_info) {
	Serial.printf("Disconnected from SSID: %s\n", event_info.ssid.c_str());
	Serial.printf("Reason: %d\n", event_info.reason);
}

bool processSyncEvent(NTPSyncEvent_t ntpEvent) {
  if (ntpEvent == timeSyncd){
    return true;
  } else {
    if (ntpEvent == noResponse)
			Serial.println("NTP server not reachable");
		else if (ntpEvent == invalidAddress)
			Serial.println("Invalid NTP server address");
    return false;
  }
}

boolean time_synced = false; // True if a time even has been triggered
NTPSyncEvent_t ntpEvent; // Last triggered event

// Settings JSON buffer
DynamicJsonBuffer json_buffer;

void setDefaultValues(JsonObject& root){
  JsonObject& wifi = root.createNestedObject("wifi");
  wifi["ssid"] = WIFI_SSID;
  wifi["password"] = WIFI_PWD;

  JsonObject& light = root.createNestedObject("lights");
  JsonArray& light_on = light.createNestedArray("on");
  light_on.add(8);
  light_on.add(0);
  JsonArray& light_off = light.createNestedArray("off");
  light_off.add(0);
  light_off.add(0);

  JsonObject& air = root.createNestedObject("air");
  JsonArray& air_on = air.createNestedArray("on");
  air_on.add(2);
  air_on.add(0);
  JsonArray& air_off = air.createNestedArray("off");
  air_off.add(10);
  air_off.add(0);

  JsonObject& watercycles = root.createNestedObject("watercycles");
  JsonArray& cycle_0 = watercycles.createNestedArray("0");
  cycle_0.add(0);
  cycle_0.add(0);
  JsonArray& cycle_1 = watercycles.createNestedArray("1");
  cycle_1.add(6);
  cycle_1.add(0);
  JsonArray& cycle_2 = watercycles.createNestedArray("2");
  cycle_2.add(12);
  cycle_2.add(0);
  JsonArray& cycle_3 = watercycles.createNestedArray("3");
  cycle_3.add(18);
  cycle_3.add(0);

  root["waterfill"] = 180 * 1000;
}


void updateSettings(JsonObject& root){
  JsonObject& wifi = root.createNestedObject("wifi");
  wifi["ssid"] = wifi_ssid;
  wifi["password"] = wifi_password;

  JsonObject& light = root.createNestedObject("lights");
  JsonArray& light_on = light.createNestedArray("on");
  light_on.add(light_time_on[0]);
  light_on.add(light_time_on[1]);
  JsonArray& light_off = light.createNestedArray("off");
  light_off.add(light_time_off[0]);
  light_off.add(light_time_off[0]);

  JsonObject& air = root.createNestedObject("air");
  JsonArray& air_on = air.createNestedArray("on");
  air_on.add(air_time_on[0]);
  air_on.add(air_time_on[1]);
  JsonArray& air_off = air.createNestedArray("off");
  air_off.add(air_time_off[0]);
  air_off.add(air_time_off[0]);

  JsonObject& watercycles = root.createNestedObject("watercycles");
  JsonArray& cycle_0 = watercycles.createNestedArray("0");
  cycle_0.add(water_cycles[0][0]);
  cycle_0.add(water_cycles[0][1]);
  JsonArray& cycle_1 = watercycles.createNestedArray("1");
  cycle_1.add(water_cycles[1][0]);
  cycle_1.add(water_cycles[1][1]);
  JsonArray& cycle_2 = watercycles.createNestedArray("2");
  cycle_2.add(water_cycles[2][0]);
  cycle_2.add(water_cycles[2][1]);
  JsonArray& cycle_3 = watercycles.createNestedArray("3");
  cycle_3.add(water_cycles[3][0]);
  cycle_3.add(water_cycles[3][1]);

  root["waterfill"] = water_fill_time;
}

void applyInitialSettings(JsonObject& root){
  // Water fill time
  water_fill_time = root["waterfill"];  // seconds * millis

  // Light timers
  light_time_on[0] = root["lights"]["on"][0];
  light_time_on[1] = root["lights"]["on"][1];

  light_time_off[0] = root["lights"]["off"][0];
  light_time_off[1] = root["lights"]["off"][1];

  // air timers
  air_time_on[0] = root["air"]["on"][0];
  air_time_on[1] = root["air"]["on"][1];

  air_time_off[0] = root["air"]["off"][0];
  air_time_off[1] = root["air"]["off"][1];

  // Water cycles
  water_cycles[0][0] = root["watercycles"]["0"][0];
  water_cycles[0][1] = root["watercycles"]["0"][1];
  water_cycles[1][0] = root["watercycles"]["1"][0];
  water_cycles[1][1] = root["watercycles"]["1"][1];
  water_cycles[2][0] = root["watercycles"]["2"][0];
  water_cycles[2][1] = root["watercycles"]["2"][1];
  water_cycles[3][0] = root["watercycles"]["3"][0];
  water_cycles[3][1] = root["watercycles"]["3"][1];

  // WiFi
  wifi_ssid = root["wifi"]["ssid"];
  wifi_password = root["wifi"]["password"];
}

void readSettings(){
    File f = SPIFFS.open("/hydrosettings.json", "r");
    if (!f){
      Serial.println("No settings file found. Creating initial defaults");
      f.close();
      JsonObject& root = json_buffer.createObject();
      setDefaultValues(root);
      writeSettings(root);
    } else {
      // Read settings
      String json = f.readStringUntil('\n');
      JsonObject& root = json_buffer.parseObject(json.c_str());
      f.close();
      if (!root.success()){
        Serial.println("Parsing json failed!");
      }
      else {
        Serial.println("Parsing json succeeded!");
        root.printTo(Serial);
        applyInitialSettings(root);
      }
    }
  }

void writeSettings(JsonObject& root){
  // Save settings
  File f = SPIFFS.open("/hydrosettings.json", "w");
  root.printTo(f);
  f.close();
  Serial.println("Wrote settings to disk:");
  root.printTo(Serial);
}


void setup()
{
  // Init pins
  pinMode(WATER_PUMP_PIN, OUTPUT);
  digitalWrite(WATER_PUMP_PIN, LOW);
  pinMode(AIR_PUMP_PIN, OUTPUT);
  digitalWrite(AIR_PUMP_PIN, LOW);
  pinMode(MOSFET_3, OUTPUT);
  digitalWrite(MOSFET_3, LOW);

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

  // load settings
  SPIFFS.begin();
  readSettings();

  // initialize last_fill_start
  last_fill_start = millis();

  NTP.onNTPSyncEvent([](NTPSyncEvent_t event) {
		ntpEvent = event;
		time_synced = processSyncEvent(ntpEvent);
    if (time_synced){
      if (!mqtt_client.connected())
        startMQTT();
    }
	});

  static WiFiEventHandler e1, e2;
  WiFi.begin(wifi_ssid, wifi_password);
  e1 = WiFi.onStationModeGotIP(onSTAGotIP);// As soon WiFi is connected, start NTP Client
	e2 = WiFi.onStationModeDisconnected(onSTADisconnected);

}

void initAlarms(){
  static bool alarms_set = false;
  if (timeStatus() == timeSet && !alarms_set){
    // Initial Alarm setup
    Alarm.alarmRepeat(water_cycles[0][0], water_cycles[0][1], 0, waterOn);
    Alarm.alarmRepeat(water_cycles[1][0], water_cycles[1][1], 0, waterOn);
    Alarm.alarmRepeat(water_cycles[2][0], water_cycles[2][1], 0, waterOn);
    Alarm.alarmRepeat(water_cycles[3][0], water_cycles[3][1], 0, waterOn);
    Alarm.timerRepeat(0, CHECK_WATER_INTERVAL, 0, checkWateringStatus);
    alarms_set = true;
  }
}

void loop() {
  initAlarms();
  ArduinoOTA.handle();
  //rotary.loop();
  mqtt_client.loop();
  checkLight();
  checkAirPump();
  phoneHome();
  Alarm.delay(0);
}

void phoneHome(){
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
  bool modified = false;

  value.reserve(length + 1);
  for (unsigned int i = 0; i < length; i++) {
    value += static_cast<char>(payload[i]);
  }

  if (topic.endsWith("/light-on")) {
    if (parseTime(value, light_time_on[0], light_time_on[1])) {
      Serial.print("Setting new light on time: ");
      Serial.println(value);
      modified = true;
    }
  } else if (topic.endsWith("/light-off")) {
    if (parseTime(value, light_time_off[0], light_time_off[1])) {
      Serial.print("Setting new light off time: ");
      Serial.println(value);
      modified = true;
    }
  } else if (topic.endsWith("/air-on")) {
    if (parseTime(value, air_time_on[0], air_time_on[1])) {
      Serial.print("Setting new air on time: ");
      Serial.println(value);
      modified = true;
    }
  } else if (topic.endsWith("/air-off")) {
    if (parseTime(value, air_time_off[0], air_time_off[1])) {
      Serial.print("Setting new air off time: ");
      Serial.println(value);
      modified = true;
    }
  } else if (topic.endsWith("/water/filltime")) {
    long filltime = value.toInt();
    if (filltime != 0 && filltime > 0) {
      Serial.print("Setting new water fill time: ");
      Serial.println(value);
      water_fill_time = value.toInt() * 1000;
      modified = true;
    }
  } else if (topic.endsWith("/status")){
    Serial.println("Status request");
    JsonObject& root = json_buffer.createObject();
    updateSettings(root);
    String status;
    root["waterfill"].printTo(status);
    mqtt_client.publish("public/hydroponics-1/status-result", status.c_str());
    mqtt_client.publish("public/hydroponics-1/status-result", "status");
    Serial.println(status);
  } else if (topic.indexOf("/watercycle/") != -1){
    int index = topic.indexOf("/watercycle/");
    int cycle_num = topic.substring(index + 12).toInt();

    if (cycle_num < -1 || cycle_num > 3)
      return;

    if (cycle_num == -1){
      Serial.println("Toggle Water!");
      if (water_on)
        waterOff();
      else
        waterOn();

      // Water NOW or for value amount of time
    } else if (parseTime(value, water_cycles[cycle_num][0], water_cycles[cycle_num][1])){
      Serial.print("Setting new time for water cycle number ");
      Serial.print(cycle_num);
      Serial.print(": ");
      Serial.println(value);
      Serial.print("Alarm time before: ");
      Serial.println(Alarm.read(cycle_num));
      Alarm.write(cycle_num, AlarmHMS(water_cycles[cycle_num][0], water_cycles[cycle_num][1], 0));
      Serial.print("Alarm time after: ");
      Serial.println(Alarm.read(cycle_num));
      modified = true;
    }
  }
  if (modified){
    JsonObject& root = json_buffer.createObject();
    updateSettings(root);
    writeSettings(root);
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
  if (!light_on) {
    Serial.println("Light on, Good morning!");
    Nexa.Device_On(0);
    light_on = true;
  }
}

// Light timer
void lightOff(){
  if (light_on) {
    Serial.println("Light Off, Good nights!");
    Nexa.Device_Off(0);
    light_on = false;
  }
}


// Air On
void airOn(){
  if (!air_on) {
    Serial.println("Air Pump On, I can breathe!");
    digitalWrite(AIR_PUMP_PIN, HIGH);
    air_on = true;
  }
}

// Air Off
void airOff(){
  if (air_on) {
    Serial.println("Air Pump Off, Enough fresh air!");
    digitalWrite(AIR_PUMP_PIN, LOW);
    air_on = false;
  }
}


void checkLight() {
  if (!time_synced)
    return ;

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


void checkAirPump() {
  if (!time_synced)
    return ;

  if (air_time_off[0] < air_time_on[0]) {
      if (hour() >= air_time_on[0] && minute() >= air_time_on[1])
        airOn();
      else
        airOff();
  }
  else {
      if (hour() >= air_time_on[0] && hour() <= air_time_off[0]) {
        if (minute() >= air_time_on[1] && minute() < air_time_off[1])
          airOn();
        else
          airOff();

      } else {
        airOff();
      }
    }
}


void waterOn(){
  if (water_on){
    Serial.println("Water is already on");
  } else {
    Serial.println("Turn on water");
    digitalWrite(WATER_PUMP_PIN, HIGH);
    Alarm.timerOnce(water_fill_time / 1000, waterOff);
    last_fill_start = millis();
    water_on = true;
  }
}

void waterOff(){
  if (water_on) {
    Serial.println("Water OFF");
    digitalWrite(WATER_PUMP_PIN, LOW);
    water_on = false;
  }
}

void checkWateringStatus(){
  unsigned long diff = millis() - last_fill_start;
  bool pumping = (digitalRead(WATER_PUMP_PIN) == HIGH);

  if (pumping && diff > water_fill_time){
    Serial.println("Water on passed fill time ");
    waterOff();
  }
  else if (pumping && diff < water_fill_time) {
    Serial.println("Within fill cycle, all OK");
  }
  else if (!pumping && water_on && diff < water_fill_time) {
    Serial.println("Within fill cycle, resume OK");
    waterOn();
  }
  else {
    waterOff();
    Serial.println("Water off, all OK");
  }
}
