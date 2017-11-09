#define DEVICE_ID "hydroponics-1"

#define MQTT_ADDRESS "mqtt.bitraf.no"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID DEVICE_ID

#define FORMAT_SPIFFS false


// NodeMCU
#if defined ARDUINO_ESP8266_NODEMCU
  // RF transmit
  #define RF_DATA D4

  // Switches
  #define SWITCH_1 D0
  #define SWITCH_2 D5
  #define SWITCH_3 D6

  // Rotary encoder
  #define ROT_0 D3
  #define ROT_1 D2
  #define ROT_SW D1

  // Buzzer
  #define BUZZ D7

  // Temp hum
  #define TEMP_HUM D8

  // LDR
  #define LDR A0

// ESP8266 Chip only
#elif defined ARDUINO_ESP8266_ESP01
  // RF transmit
  #define RF_DATA 11

  // Switches
  #define SWITCH_1 4
  #define SWITCH_2 5
  #define SWITCH_3 6

  // Rotary encoder
  #define ROT_0 12
  #define ROT_1 13
  #define ROT_SW 14

  // Buzzer
  #define BUZZ 7

  // Temp hum
  #define TEMP_HUM 10

  // LDR
  #define LDR 2

#endif
