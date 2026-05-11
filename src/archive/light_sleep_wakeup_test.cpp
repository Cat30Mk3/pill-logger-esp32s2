#include <Arduino.h>
#include <esp_sleep.h>

#define BUTTON_PIN 39   // GPIO39 (RTC IO)
#define LED_PIN 15      // Any available GPIO for LED

void setup() {
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT); // Use external pull-up
    // Blink LED for 5 seconds before sleep
    for (int i = 0; i < 10 * 5; i++) { // 5 seconds, 100ms per blink
        digitalWrite(LED_PIN, HIGH);
        delay(50);
        digitalWrite(LED_PIN, LOW);
        delay(50);
    }
    // Enable GPIO wakeup on BUTTON_PIN (LOW level)
    esp_sleep_enable_gpio_wakeup();
    gpio_wakeup_enable((gpio_num_t)BUTTON_PIN, GPIO_INTR_LOW_LEVEL);
    // Turn off LED before sleep
    digitalWrite(LED_PIN, LOW);
    delay(100);
    esp_light_sleep_start();
}

void loop() {
    // After waking from light sleep, blink LED continuously to confirm wakeup
    while (1) {
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
    }
}
