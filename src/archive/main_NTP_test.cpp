#include <Arduino.h>
#include "globals.h"
#include "core_system.h"
#include "time_logic.h"
#include "data_store.h"
#include "display_ui.h"

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, LOW);

#if SERIAL_DEBUG
    DBG_DELAY(2000);
    Serial.begin(115200);
    while (!Serial && millis() < 5000)
    {
        DBG_DELAY(100);
    }

    digitalWrite(LED_BUILTIN, HIGH);
    DBG_DELAY(5000); // Wait for USB CDC to enumerate
    DBG_PRINTLN("");
    DBG_PRINTLN("Starting Pill Logger - Milestone 2 baseline");
#endif

    initDataStore();
    initCoreSystem();
    initDisplay();
    initWiFi();

    persistent_data_dump("After initDataStore");
    rtc_dump("After rtc_load");

    lastActivityTime = millis();

    in_setup_assess_ntp_sync_needed(esp_sleep_get_wakeup_cause());

    DBG_PRINTLN("Setup complete. Entering main loop...");
    DBG_FLUSH();
    digitalWrite(LED_BUILTIN, HIGH);
}

void loop()
{
    uint32_t currentTime = millis();

    processButtonEvents();

    if (isShortPress())
    {
        DBG_PRINTLN("short press detected - advancing screen");
        nextScreen();
        lastActivityTime = currentTime;
    }

    if (isLongPress())
    {
        DBG_PRINTLN("long press detected");
        // Add handling for long press here
        lastActivityTime = currentTime;
    }

    renderScreen(getScreenIndex());

    in_loop_assess_ntp_sync_needed();
    isNewLocalDay();

    if ((currentTime - lastActivityTime) >= INACTIVITY_TIMEOUT_MS)
    {
#if DEEP_SLEEP_ENABLE
        persistence_test_increment();                                              // Debug: Increment PersistentData to test persistence on next boot
        persistence_save();                                                        // Debug: Save incremented PersistentData to NVS immediately for testing
        persistent_data_dump("Before entering deep sleep - PersistentData state"); // Debug dump before sleep
        rtc_save();                                                                // Save RTC state to fast memory before sleep
        rtc_dump("Before entering deep sleep - RTC state");                        // Debug dump of RTC state before sleep

        u8g2.setPowerSave(1); // Put display to sleep
        enterDeepSleep();     // Enter deep sleep (function will call esp_deep_sleep_start())
#endif

        lastActivityTime = currentTime;
        DBG_PRINTLN("Inactivity timeout reached - would enter deep sleep here");
    }

    // Keep LED flashing in this baseline for active-loop visibility.
    if (millis() % 500 < 250)
    {
        digitalWrite(LED_BUILTIN, HIGH);
    }
    else
    {
        digitalWrite(LED_BUILTIN, LOW);
    }

    delay(10);
}
