#include <Arduino.h>

void setup() {
    Serial.begin(115200);
    delay(5000);
    Serial.println("S2 Mini serial OK");
}

void loop() {
    Serial.println("tick");
    delay(1000);
}