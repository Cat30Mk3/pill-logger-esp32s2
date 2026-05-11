#include <Arduino.h>
#include <esp_sleep.h>

#define BUTTON_PIN 1 // GPIO1 (RTC IO)
#define LED_PIN 15   // Any available GPIO for LED

void setup()
{
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT); // Use external pull-up

    // Check wakeup reason
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
    if (cause == ESP_SLEEP_WAKEUP_EXT1)
    {
        // Blink LED rapidly to indicate ext1 wake
        for (int i = 0; i < 10; i++)
        {
            digitalWrite(LED_PIN, HIGH);
            delay(100);
            digitalWrite(LED_PIN, LOW);
            delay(100);
        }
    }
    else
    {
        // Blink LED slowly to indicate power-on or other wake
        for (int i = 0; i < 3; i++)
        {
            digitalWrite(LED_PIN, HIGH);
            delay(500);
            digitalWrite(LED_PIN, LOW);
            delay(500);
        }
    }

    delay(5000); // Wait a moment before sleep

    esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0); // 0 = wake on LOW

    // Turn off LED before sleep
    digitalWrite(LED_PIN, LOW);
    delay(100);
    esp_deep_sleep_start();
}

void loop()
{
    // Not used
}
