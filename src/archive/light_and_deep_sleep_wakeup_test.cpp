#include <Arduino.h>
#include <esp_sleep.h>

#define BUTTON_PIN 1 // GPIO1 (RTC IO) - CONFIRMED !! THIS MUST A PIN BETWEEN GPIO1 and GPIO21
#define LED_PIN 15    // built-in LED

#define USE_LIGHT_SLEEP 0  // Set to 1 for light sleep, 0 for deep sleep


void setup()
{
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_PIN, INPUT); // Use external pull-up
    // Blink LED for 5 seconds before sleep
    for (int i = 0; i < 10 * 5; i++)
    { // 5 seconds, 100ms per blink
        digitalWrite(LED_PIN, HIGH);
        delay(50);
        digitalWrite(LED_PIN, LOW);
        delay(50);
    }
        // Enable wakeup on BUTTON_PIN (LOW level)
    #if USE_LIGHT_SLEEP
        esp_sleep_enable_gpio_wakeup();
        gpio_wakeup_enable((gpio_num_t)BUTTON_PIN, GPIO_INTR_LOW_LEVEL);
    #else
        // Use ext0 for deep sleep (single pin, level-sensitive)
        esp_sleep_enable_ext0_wakeup((gpio_num_t)BUTTON_PIN, 0); // 0 = wake on LOW
    #endif
    // Turn off LED before sleep
    digitalWrite(LED_PIN, LOW);
    delay(100);
    // Select sleep mode
#if USE_LIGHT_SLEEP
    esp_light_sleep_start();
#else
    esp_deep_sleep_start();
#endif
}

void loop()
{
#if USE_LIGHT_SLEEP
    // After waking from light sleep, blink LED continuously to confirm wakeup
    while (1)
    {
        digitalWrite(LED_PIN, HIGH);
        delay(100);
        digitalWrite(LED_PIN, LOW);
        delay(100);
    }
#else
    // In deep sleep, setup() will run again after wake, so loop does nothing
    while (1)
    {
    }
#endif
}
