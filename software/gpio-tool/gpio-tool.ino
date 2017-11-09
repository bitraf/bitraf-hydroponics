#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>

#include "misc.h"

SerialReader readline;

#include "wifipassword.h" // Make this file if it doesn't exist. It will not be checked into Git.
const char *ssid = WIFI_SSID;
const char *password = WIFI_PWD;

const char *device_id = "gpio-tool";

fixed_interval_timer<5000> wifi_connect_timer;
bool wifi_connected = false;

ESP8266WebServer server(80);

void handleRoot()
{
    char temp[400];
    int sec = millis() / 1000;
    int min = sec / 60;
    int hr = min / 60;

    snprintf(temp, 400,
"<html>\
  <head>\
    <meta http-equiv='refresh' content='5'/>\
    <title>ESP8266 Demo</title>\
    <style>\
      body { background-color: #cccccc; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }\
    </style>\
  </head>\
  <body>\
    <h1>Hello from ESP8266!</h1>\
    <p>Uptime: %02d:%02d:%02d</p>\
    <img src=\"/test.svg\" />\
  </body>\
</html>", hr, min % 60, sec % 60);
    server.send(200, "text/html", temp);
}

void handleNotFound()
{
    String message = "File Not Found\n\n";
    message += "URI: ";
    message += server.uri();
    message += "\nMethod: ";
    message += (server.method() == HTTP_GET) ? "GET" : "POST";
    message += "\nArguments: ";
    message += server.args();
    message += "\n";

    for (uint8_t i = 0; i < server.args(); i++) {
        message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
    }

    server.send(404, "text/plain", message);
}

void setup(void)
{
    Serial.begin(115200);
    Serial.print("\r\n\r\n\r\nBooting...");
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    wifi_connect_timer.reset();

    server.on("/", handleRoot);
    server.on("/gpio", []() {
        Serial.printf("GPIO: args=%d\r\n", server.args());
        for (int i = 0; i < server.args(); i++) {
            Serial.printf("#%d: %s=%s\r\n", i, server.argName(i).c_str(), server.arg(i).c_str());
        }
        int pin = -1;
        for (int i = 0; i < server.args(); i++) {
            if (server.argName(i) == "pin") {
                pin = server.arg(i).toInt();
            }
        }
        Serial.printf("pin=%d\r\n", pin);

        Serial.printf("has pin: %d\r\n", server.hasArg("pin"));
        Serial.printf("pin=%s\r\n", server.arg("pin").c_str());
        server.send(404, "text/plain", "gpio, yo");
    });
    server.onNotFound(handleNotFound);
    server.begin();
    Serial.println("HTTP server started");


    ArduinoOTA.onStart([]() {
        Serial.println("Start");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("\nEnd");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
        else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
        else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
        else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
        else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });
    ArduinoOTA.setHostname(device_id);
    ArduinoOTA.begin();
    Serial.println("OTA configured");
}

void loop()
{
    ArduinoOTA.handle();

    server_loop();

    static SerialReader reader;
    String line;
    if (reader.read(line)) {
        execute(line);
    }
}

static void server_loop()
{

    bool tmp = WiFi.status() == WL_CONNECTED;

    if (tmp) {
        if (!wifi_connected) {
            wifi_connected = true;
            Serial.printf("Connected to %s\r\n", ssid);
            Serial.printf("IP address: %s\r\n", WiFi.localIP().toString().c_str());
        }

        server.handleClient();

        return;
    }

    if (!wifi_connect_timer.expired()) {
        return;
    }

    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    ESP.restart();
}

static void execute(const String &line)
{
    Serial.println("Got line");
    Serial.println(line);

    auto idx1 = line.indexOf(' ');
    if (idx1 < 1) {
        Serial.println("cmd format: [gpio number] [on|off|read]");
        return;
    }

    int idx2 = line.indexOf(' ', idx1 + 1);

    if (idx2 < 0) {
        idx2 = line.length();
    }

    int gpio = line.substring(0, idx1).toInt();
    String cmd = line.substring(idx1 + 1, idx2);

    if (cmd == "on") {
        pinMode(gpio, OUTPUT);
        digitalWrite(gpio, HIGH);
        Serial.printf("Setting %d HIGH\r\n", gpio);
    } else if (cmd == "off") {
        pinMode(gpio, OUTPUT);
        digitalWrite(gpio, LOW);
        Serial.printf("Setting %d LOW\r\n", gpio);
    } else if (cmd == "read") {
        pinMode(gpio, INPUT);
        int value = digitalRead(gpio);
        Serial.printf("value: %d\r\n", value);
    } else {
        Serial.println("Unknown command");
    }
}

