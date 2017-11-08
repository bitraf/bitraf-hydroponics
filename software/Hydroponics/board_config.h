#define DEVICE_ID "hydroponics-1"

#define MQTT_ADDRESS "mqtt.bitraf.no"
#define MQTT_PORT 1883
#define MQTT_CLIENT_ID DEVICE_ID

#define FORMAT_SPIFFS false

// GPIO pins

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
