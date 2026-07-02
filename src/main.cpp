// This is Milestone 4-E - working?? with setup screens

#include <Arduino.h>
#include <esp_sleep.h>
#include "globals.h"
#include "core_system.h"
#include "time_logic.h"
#include "data_store.h"
#include "display_ui.h"
#include "battery.h"
#include <SPI.h>

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
#if LED_ENABLE
    digitalWrite(LED_BUILTIN, LOW);
#endif

    // Get wake cause early to determine setup path
    esp_sleep_wakeup_cause_t wakeCause = esp_sleep_get_wakeup_cause();

    // Initialize display early (before Serial/CDC delay) if NOT a timer wake
    if (wakeCause != ESP_SLEEP_WAKEUP_TIMER)
    {
        initDisplay();
        g_current_setup_state = SETUP_STATE_SERIAL_WAIT;
        renderSetupScreen(SETUP_INITIALIZATION);
    }

#if SERIAL_DEBUG_ENABLE
    renderSetupScreen(SETUP_STATE_SERIAL_WAIT);
    DBG_DELAY(2000);
    Serial.begin(115200);
    while (!Serial && millis() < 5000)
    {
        DBG_DELAY(100);
    }

#if LED_ENABLE
    digitalWrite(LED_BUILTIN, HIGH);
#endif
    DBG_DELAY(5000); // Wait for USB CDC to enumerate
    DBG_PRINTLN("");
    DBG_PRINT("Starting Pill Logger - ");
    DBG_PRINTLN(VERSION_NAME);
    renderSetupScreen(SETUP_INITIALIZATION);
#endif

    // Initialize minimum required systems early (needed for both timer and normal wakes)
    initDataStore();
    initCoreSystem();

    // Timer wake runs housekeeping only, keeps display off, and returns to sleep.
    if (wakeCause == ESP_SLEEP_WAKEUP_TIMER)
    {
        // Assess NTP sync need BEFORE touching WiFi hardware
        in_setup_assess_ntp_sync_needed(wakeCause);

        // Only power up WiFi if NTP sync is actually needed (saves power on most wakes)
        if (!rtc_fast_state.live_clock_synced)
        {
            initWiFi();
        }

        // Attempt NTP sync if needed (WiFi initialized above only when required)
        in_loop_assess_ntp_sync_needed();

        // isNewLocalDay() calls savePersistentData() internally if day changed - no extra save needed
        isNewLocalDay();

#if DEEP_SLEEP_ENABLE
        rtc_save();
        enterDeepSleep();
#endif
    }

    // Normal boot path from here (non-timer wake)
    g_current_setup_state = SETUP_INITIALIZATION;
    renderSetupScreen(SETUP_INITIALIZATION);
    delay(1000); // Increase delay so initialization screen is readable

    initWiFi();

    delay(500); // Brief delay to make setup screens visible for debugging

    initBattery();

    // Enable PB_TOP button detection after WiFi/NTP initialization complete
    // (prevents spurious pill-taken events during setup)
    pb_top_enable();

    preloadPersistentTestDataIfEnabled(wakeCause);

    // Measure battery voltage on all non-timer wakeups
    // (timer wakes skip to avoid unnecessary delay with display off)
    if (wakeCause != ESP_SLEEP_WAKEUP_TIMER)
    {
        readBatteryVoltage();
        batteryDumpStatus();
    }

    persistent_data_dump("After initDataStore");
    rtc_dump("After rtc_load");

    lastActivityTime = millis();

    // Assess NTP sync need based on wake cause (sets RTC flags)
    // Actual NTP sync will happen in main loop
    in_setup_assess_ntp_sync_needed(wakeCause);

    g_current_setup_state = SETUP_STATE_READY;
    renderSetupScreen(SETUP_STATE_READY);
    delay(1000); // Brief delay to make setup screens visible for debugging
    g_setup_complete = true;

    if (wakeCause != ESP_SLEEP_WAKEUP_TIMER)
    {
        // Low battery alert: if SOC is at or below 20% and not on USB power,
        // start on the battery status screen (screen 4) so the user sees
        // the low-battery warning immediately on wake/boot.
        if (!isUsbPowered() && getBatterySoc() <= 20)
        {
            setScreenIndex(4);
        }
        else
        {
            setScreenIndex(0);
        }
        u8g2.setPowerSave(0);
    }

    // Transition to Screen 0 (Pills Today)
    renderScreen(getScreenIndex());

    // g_edit_mode_active = true; // Uncomment to test PB_TOP gate (T6) - screen/pill-log actions must be suppressed while true

    DBG_PRINTLN("Setup complete. Entering main loop...");
    DBG_FLUSH();
#if LED_ENABLE
    digitalWrite(LED_BUILTIN, HIGH);
#endif
}

void loop()
{
    uint32_t currentTime = millis();

    // Button processing (timer-based for PB_TOP, ISR-based for nav buttons)
    processNavButtonEvents(); // Milestone 4: nav buttons for edit menus

    if (g_edit_mode_active)
    {
        // --- Edit mode: all input goes to MUI; PB_TOP is silently gated ---
        processMuiInput();
        renderEditScreen();
    }
    else
    {
        // --- Normal mode: check for edit entry, then handle PB_TOP + primary screen ---
        checkMuiEntry();

        if (isConfirmationPopupActive())
        {
            bool shortPress = pb_top_is_short_press();
            bool longPress = pb_top_is_long_press();
            if (shortPress || longPress)
            {
                clearConfirmationPopup();
                setScreenIndex(0);
                lastActivityTime = millis();
                persistent_data_dump("Popup dismissed by button press");
            }
        }
        else
        {
            if (pb_top_is_short_press())
            {
                DBG_PRINTLN("short press detected - advancing screen");
                nextScreen();
                lastActivityTime = millis();
            }

            if (pb_top_is_long_press())
            {
                DBG_PRINTLN("[MAIN] *** LONG PRESS DETECTED IN MAIN LOOP ***");
                if (getScreenIndex() == 0)
                {
                    DBG_PRINTLN("[MAIN] Processing pill taken (screen 0)");
                    processPillTakenLongPress();
                    lastActivityTime = millis();
                }
                else
                {
                    DBG_PRINTF("[MAIN] Long press on screen %u - silently ignored by directive\n", (unsigned)getScreenIndex());
                }
            }
        }

        renderScreen(getScreenIndex());
    }

    in_loop_assess_ntp_sync_needed();
    isNewLocalDay();

    uint32_t inactivityNow = millis();
    if ((inactivityNow - lastActivityTime) >= INACTIVITY_TIMEOUT_MS)
    {
        // If timeout hits during edit UX transitions, abandon edit mode first
        // and defer sleeping to a later loop to avoid immediate unintended sleep.
        if (g_edit_mode_active)
        {
            DBG_PRINTF("[EDIT] inactivity timeout in edit mode after %lu ms - abandoning edit mode before sleep\n",
                       (unsigned long)(inactivityNow - lastActivityTime));
            exitEditMode(false);
            // Do NOT reset lastActivityTime — next loop g_edit_mode_active is
            // false and the inactivity check fires immediately, entering sleep.
            return;
        }

        DBG_PRINTF("[SLEEP] inactivity timeout reached after %lu ms on screen %u\n",
                   (unsigned long)(inactivityNow - lastActivityTime),
                   (unsigned)getScreenIndex());

#if DEEP_SLEEP_ENABLE
        // persistence_test_increment();                                            // Debug: Increment PersistentData to test persistence on next boot
        persistence_save();                                                        // Debug: Save incremented PersistentData to NVS immediately for testing
        persistent_data_dump("Before entering deep sleep - PersistentData state"); // Debug dump before sleep
        rtc_save();                                                                // Save RTC state to fast memory before sleep
        rtc_dump("Before entering deep sleep - RTC state");                        // Debug dump of RTC state before sleep

        u8g2.setPowerSave(1); // Put display to sleep
        enterDeepSleep();     // Enter deep sleep (function will call esp_deep_sleep_start())
#endif

        lastActivityTime = inactivityNow;
        DBG_PRINTLN("Inactivity timeout reached - would enter deep sleep here");
    }

    // Keep LED flashing in this baseline for active-loop visibility.
#if LED_ENABLE
    if (millis() % 500 < 250)
    {
        digitalWrite(LED_BUILTIN, HIGH);
    }
    else
    {
        digitalWrite(LED_BUILTIN, LOW);
    }
#endif

    delay(10);
}
