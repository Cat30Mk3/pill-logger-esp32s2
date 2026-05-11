#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "esp_wifi.h"


const char* ssid     = "JohnAndLinda";
const char* password = "7054563991";
const char* ntpServer = "pool.ntp.org";
const char* tzInfo = "EST5EDT,M3.2.0/2,M11.1.0/2";



void setup() {

    Serial.begin(115200);
    delay(300);

    Serial.println("\nResetting WiFi...");
    WiFi.disconnect(true, true);
    delay(200);

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);

    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid, password);

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000) {
        Serial.print(".");
        delay(250);
    }

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\nWiFi FAILED to connect.");
        return;
    }

    Serial.println("\nWiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    configTime(0, 0, ntpServer);
    setenv("TZ", tzInfo, 1);
    tzset();

    Serial.println("Waiting for time sync...");
    delay(2000);
}

void loop() {
    time_t now = time(nullptr);
    struct tm localTime;
    localtime_r(&now, &localTime);

    Serial.printf("Local time: %04d-%02d-%02d  %02d:%02d:%02d\n",
                  localTime.tm_year + 1900,
                  localTime.tm_mon + 1,
                  localTime.tm_mday,
                  localTime.tm_hour,
                  localTime.tm_min,
                  localTime.tm_sec);

    delay(1000);
}