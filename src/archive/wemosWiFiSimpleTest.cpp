#include <Arduino.h>
#include <WiFi.h>

const char* ssid = "JohnAndLinda";
const char* password = "7054563991";

void setup() {
  Serial.begin(115200);
  delay(5000);

  Serial.println("Connecting...");
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  WiFi.begin(ssid, password);

  WiFi.setTxPower(WIFI_POWER_8_5dBm);

  while (WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(500);
  }

  Serial.println("Connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void loop() {
      Serial.println("looping");
      delay(2000);
}