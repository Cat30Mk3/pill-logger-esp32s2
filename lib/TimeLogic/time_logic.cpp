
// ===================== NTP SYNC/RETRY/DEBUG STUBS =====================
#include <Arduino.h>
#include "time_logic.h"
#include "globals.h"
#include "data_store.h"
#include "display_ui.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include <esp_sleep.h>
WiFiMulti wifiMulti;

void setSystemTimeBad() {
    time_t bad_time = time(NULL) - 1000000;  // Set time 11+ days in past
    struct timeval tv;
    tv.tv_sec = bad_time;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
}

void in_setup_assess_ntp_sync_needed(esp_sleep_wakeup_cause_t cause)
{
//setSystemTimeBad(); // Debug: Force bad system time to test NTP sync logic in setup

    // for call within setup() to assess whether NTP sync is needed and setup RTC state accordingly based on wakeup cause and time since last sync
    if (cause == ESP_SLEEP_WAKEUP_TIMER || cause == ESP_SLEEP_WAKEUP_EXT0 || cause == ESP_SLEEP_WAKEUP_EXT1) // did we wake from deepSleep?
    {
        DBG_PRINTLN("[In_setup] Woke from deep sleep, assessing if NTP sync needed");
        rtc_fast_state.last_ntp_sync_reason = 0;
        printLocalTime();
        //    if (getLocalTime_Tm().tm_year < 2026) // Is system time bad? *** see future enhancement note above - this is a crude check for invalid time based on compile time, but we should implement a more robust solution in the future
        if (getUtcTime() < pdata.last_compile_timestamp) // Is system time bad? *** see future enhancement note above - this is a crude check for invalid time based on compile time, but we should implement a more robust solution in the future
        {
            DBG_PRINTLN("[In_setup] System time appears to be invalid, will attempt NTP sync");
            rtc_fast_state.last_ntp_sync_reason = 1; // 1 = sync needed due to invalid system time
            printLocalTime();
            DBG_PRINT("[In_setup] Current UTC time: ");
            DBG_PRINTLN(getUtcTime());
            DBG_PRINT("[In_setup] Current UTC time: ");
            printUTCTime();
            DBG_PRINT("[In_setup] compile time: ");
            DBG_PRINTLN(pdata.last_compile_timestamp);
            rtc_fast_state.live_clock_synced = false;
            return;
        }

        if (getUtcTime() - rtc_fast_state.last_ntp_sync_timestamp > NTP_REFRESH_INTERVAL_SECONDS) // is it time for periodic NTP sync?
        {
            DBG_PRINTLN("[In_setup] Periodic NTP sync needed");
            rtc_fast_state.last_ntp_sync_reason = 2; // 2 = sync needed due to periodic refresh
            rtc_fast_state.live_clock_synced = false;
            return;
        }
        rtc_fast_state.live_clock_synced = true;
        DBG_PRINTLN("[In_setup] Woke from deep sleep but NTP sync not needed yet");
        return;
    }
    else // must be a reboot, or program load
    {
        rtc_fast_state.live_clock_synced = false;
        rtc_fast_state.ntp_backoff_active = false;
        rtc_fast_state.ntp_backoff_start_ms = 0;
        rtc_fast_state.last_ntp_sync_reason = 3; // 3 = sync needed due to reboot or program load


        DBG_PRINTLN("[In_setup] Not waking from deep sleep, will attempt NTP sync");
    }
    return;
}

void in_loop_assess_ntp_sync_needed()
{
    // for call within loop() to assess whether NTP sync is needed and trigger if needed based on time since last sync
    if (!rtc_fast_state.live_clock_synced)
    {
        DBG_PRINTLN("[In_loop] Assessing NTP sync need - clock not currently synced");
        if (!rtc_fast_state.ntp_backoff_active)
        {
            DBG_PRINTLN("[In_loop] NTP sync needed and not currently in backoff, will attempt sync");
            // fall through to sync attempt logic below
        }
        else if (rtc_fast_state.ntp_backoff_active && ((millis() - rtc_fast_state.ntp_backoff_start_ms) >= NTP_RETRY_BACKOFF_INTERVAL_SECONDS * 1000))
        {
            DBG_PRINTLN("[In_loop] NTP sync needed and backoff period has expired, will attempt sync");
            rtc_fast_state.ntp_backoff_active = false; // reset backoff state to allow sync attempt
            // fall through to sync attempt logic below
        }
        else
        {
            DBG_PRINTLN("[In_loop] NTP sync needed but currently in backoff, will not attempt sync");
            return;
        }
        DBG_PRINTLN("\n[In_loop] VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV");
        DBG_PRINTLN("[In_loop] NTP Sync needed - attempting sync");
        renderSetupScreen(SETUP_STATE_NTP_SYNCING);
        rtc_fast_state.ntp_backoff_start_ms = 6451;
        persistent_data_dump("[In_loop] Before NTP sync attempt"); // Debug dump of PersistentData before sync attempt
        rtc_dump("[In_loop] Before NTP sync attempt");
        if (ntp_sync_attempt()) // this will attempt multiple times before returning false, and will update RTC state accordingly
        {
            DBG_PRINTLN("[In_loop] NTP Sync successful");
            rtc_fast_state.live_clock_synced = true;
            rtc_fast_state.ntp_backoff_start_ms = 0;
        }
        else
        {
            DBG_PRINTLN("[In_loop] NTP Sync failed - setting backoff state");
            rtc_fast_state.ntp_backoff_start_ms = millis();
            rtc_fast_state.ntp_backoff_active = true;
        }
        persistent_data_dump("[In_loop] After NTP sync attempt"); // Debug dump of PersistentData before sync attempt
        rtc_dump("[In_loop] After NTP sync attempt");
        DBG_PRINTLN("[In_loop] ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^\n");

    }
}

void initWiFi()
{
    WiFi.mode(WIFI_STA);
    DBG_PRINTLN("[NTP] Loading WiFi credentials from secrets header");
#if SECRETS_IS_TEMPLATE
    DBG_PRINTLN("[NTP] WARNING: secrets_template.h is active (dummy credentials)");
#else
    DBG_PRINTLN("[NTP] secrets.h is active");
#endif

    for (size_t i = 0; i < wifi_credentials_count; i++)
    {
        wifiMulti.addAP(wifi_credentials[i].ssid, wifi_credentials[i].password);
        DBG_PRINT("[NTP] AP[");
        DBG_PRINT(i);
        DBG_PRINT("] SSID=");
        DBG_PRINT(wifi_credentials[i].ssid);
        DBG_PRINT(" PWD=");
        DBG_PRINTLN(wifi_credentials[i].password);
    }

    rtc_fast_state.ntp_retry_count = 0;
    rtc_fast_state.ntp_backoff_count = 0;
}

boolean ntp_sync_attempt()
{
    // WiFi/NTP sync - runs in background task with watchdog disabled
    DBG_PRINTLN("[NTP] ntp_sync_attempt() begins");
    // NOTE: last_ntp_sync_timestamp is only set AFTER a successful sync (below).
    // Setting it here before the attempt would incorrectly suppress the next sync
    // for a full interval even when this attempt fails.
    rtc_fast_state.ntp_backoff_active = false;
    DBG_PRINTLN("[NTP] Connecting to strongest WiFi in list");
    
    // WiFi connection phase - try up to 3 times with 5s timeout each
    for (int attempt = 0; attempt < 3; attempt++)
    {
        int result = wifiMulti.run(5000);  // Full timeout since no watchdog pressure
        if (result == WL_CONNECTED)
        {
            DBG_PRINT("[NTP] Connected to WiFi SSID: ");
            DBG_PRINTLN(WiFi.SSID());
            strcpy(rtc_fast_state.last_wifi_ssid, WiFi.SSID().c_str());
            rtc_fast_state.last_wifi_rssi = WiFi.RSSI();
            rtc_fast_state.last_ntp_failure_cause = NTP_OK;
            break;
        }
        else
        {
            DBG_PRINT("[NTP] WiFi attempt ");
            DBG_PRINT(attempt + 1);
            DBG_PRINTLN(" failed, retrying...");
            WiFi.disconnect(false);
            delay(1000);  // 1 second between retries (safe now - no watchdog)
        }
    }
    
    // If WiFi still not connected, give up
    if (WiFi.status() != WL_CONNECTED)
    {
        DBG_PRINTLN("[NTP] Failed to connect to WiFi after all attempts");
        rtc_fast_state.last_ntp_failure_cause = NTP_WIFI_FAIL;
        rtc_fast_state.ntp_retry_count = 0;
        WiFi.disconnect();
        return false;
    }

    // NTP sync phase
    DBG_PRINTLN("[NTP] Starting NTP sync with configTime()");
    configTime(0, 0, "pool.ntp.org");
    
    // Wait for NTP with timeout
    uint32_t ntp_start = millis();
    struct tm timeinfo;
    bool time_obtained = false;
    
    while ((millis() - ntp_start) < 10000)  // 10 second timeout
    {
        if (getLocalTime(&timeinfo, 1000))  // 1 second timeout per probe
        {
            time_obtained = true;
            break;
        }
        delay(500);  // Poll every 500ms (safe - no watchdog)
    }
    
    if (time_obtained)
    {
        DBG_PRINTLN("[NTP] NTP sync successful");
        printUTCTime();
        printLocalTime();
        rtc_fast_state.last_ntp_sync_timestamp = getUtcTime();
        rtc_fast_state.last_unix = getUtcTime();
        rtc_fast_state.last_tick = (uint32_t)esp_timer_get_time() / 1000;
        rtc_fast_state.last_ntp_failure_cause = NTP_OK;
        rtc_fast_state.ntp_retry_count = 0;
        rtc_fast_state.ntp_backoff_count = 0;
        rtc_fast_state.live_clock_synced = true;
    }
    else
    {
        DBG_PRINTLN("[NTP] Failed to obtain time from NTP");
        rtc_fast_state.last_ntp_failure_cause = NTP_TIMEOUT;
        WiFi.disconnect();
        return false;
    }
    
    WiFi.disconnect();
    return true;
}

// ============ MANUAL SYNC HELPERS ============
// These split the WiFi-connect and NTP-time phases so display_ui can show
// per-phase feedback during a user-triggered manual sync from the NTP menu.

bool wifi_connect_for_ntp()
{
    DBG_PRINTLN("[NTP] wifi_connect_for_ntp() begins");
    for (int attempt = 0; attempt < 3; attempt++)
    {
        int result = wifiMulti.run(5000);
        if (result == WL_CONNECTED)
        {
            DBG_PRINT("[NTP] WiFi connected: ");
            DBG_PRINTLN(WiFi.SSID());
            strcpy(rtc_fast_state.last_wifi_ssid, WiFi.SSID().c_str());
            rtc_fast_state.last_wifi_rssi = WiFi.RSSI();
            rtc_fast_state.last_ntp_failure_cause = NTP_OK;
            return true;
        }
        DBG_PRINTF("[NTP] wifi_connect_for_ntp attempt %d failed\n", attempt + 1);
        WiFi.disconnect(false);
        delay(1000);
    }
    DBG_PRINTLN("[NTP] wifi_connect_for_ntp: all attempts failed");
    rtc_fast_state.last_ntp_failure_cause = NTP_WIFI_FAIL;
    rtc_fast_state.ntp_retry_count = 0;
    WiFi.disconnect();
    return false;
}

bool ntp_sync_only()
{
    DBG_PRINTLN("[NTP] ntp_sync_only() begins");
    configTime(0, 0, "pool.ntp.org");

    uint32_t ntp_start = millis();
    struct tm timeinfo;
    bool time_obtained = false;

    while ((millis() - ntp_start) < 10000)
    {
        if (getLocalTime(&timeinfo, 1000))
        {
            time_obtained = true;
            break;
        }
        delay(500);
    }

    if (time_obtained)
    {
        DBG_PRINTLN("[NTP] ntp_sync_only: success");
        printUTCTime();
        printLocalTime();
        rtc_fast_state.last_ntp_sync_timestamp = getUtcTime();
        rtc_fast_state.last_unix = getUtcTime();
        rtc_fast_state.last_tick = (uint32_t)esp_timer_get_time() / 1000;
        rtc_fast_state.last_ntp_failure_cause = NTP_OK;
        rtc_fast_state.ntp_retry_count = 0;
        rtc_fast_state.ntp_backoff_count = 0;
        rtc_fast_state.live_clock_synced = true;
        WiFi.disconnect();
        return true;
    }
    else
    {
        DBG_PRINTLN("[NTP] ntp_sync_only: failed (timeout)");
        rtc_fast_state.last_ntp_failure_cause = NTP_TIMEOUT;
        WiFi.disconnect();
        return false;
    }
}

// ============ NTP SYNC ============
void syncTimeIfNeeded(bool force)
{
    // Only sync if not already synced, refresh interval expired, or time is implausible
    time_t now = 0;
    time(&now);
    // Sanity threshold: 2021-01-01 (UTC 1609459200)
    bool time_invalid = (now < 1609459200);
    if (!rtc_fast_state.live_clock_synced ||
        (now - rtc_fast_state.last_ntp_sync_timestamp) > NTP_REFRESH_INTERVAL_SECONDS ||
        time_invalid || force)
    {
        DBG_PRINTLN("[NTP] syncTimeIfNeeded: triggering NTP sync (reason: not synced, interval expired, or time invalid)");
        ntp_retry_logic();
    }
}

// ============ TIME GETTERS ============

tm getLocalTime_Tm()
{
    // Return current local time in tm structure format
    // Set timezone - must be called here - env variable does not persist
    setenv("TZ", TIMEZONE_RULE, 1);
    tzset();
    time_t now_utc = 0;
    time(&now_utc);
    struct tm lt;
    localtime_r(&now_utc, &lt);
    return lt;
}

tm getUtcTime_Tm()
{
    // Return UTC time in tm structure format
    time_t now_utc = 0;
    time(&now_utc);
    struct tm ut;
    gmtime_r(&now_utc, &ut);
    return ut;
}

uint32_t getUtcTime()
{
    // Return UTC time (seconds since epoch, POSIX/UTC)
    time_t now_utc = 0;
    time(&now_utc);
    return (uint32_t)now_utc;
}

tm getLocalTimestampTime(time_t timestamp)
{
    // returns provided timestamp converted to local time in tm structure format
    setenv("TZ", TIMEZONE_RULE, 1);
    tzset();
    struct tm lt;
    localtime_r(&timestamp, &lt);
    return (lt);
}

void printLocalTime()
{
#if SERIAL_DEBUG
    // Return current local time in tm structure format
    // Set timezone - must be called here - env variable does not persist
    setenv("TZ", TIMEZONE_RULE, 1);
    tzset();
    time_t now_utc = 0;
    time(&now_utc);
    struct tm lt;
    localtime_r(&now_utc, &lt); // applies TZ rukes to convert UTC to local time

    char buf[80];
    strftime(buf, sizeof(buf), "Local Time: %Y-%m-%d %H:%M:%S %Z", &lt);
    DBG_PRINTLN(buf);
#endif
}

void printLocalTimestampTime(const char *TimeStampName, time_t timestamp)
{
#if SERIAL_DEBUG
    setenv("TZ", TIMEZONE_RULE, 1);
    tzset();

    struct tm lt;
    localtime_r(&timestamp, &lt);

    char timeStr[80];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S %Z", &lt);

    DBG_PRINT(TimeStampName);
    DBG_PRINT(": ");
    DBG_PRINTLN(timeStr);
#endif
}

void printUTCTime()
{
#if SERIAL_DEBUG
    // Return current UTC time in tm structure format
    time_t now_utc = 0; // raw UTC epoch time
    struct tm utcTime;  // struct tm filled with pure UTC

    time(&now_utc);               // get current system time (UTC)
    gmtime_r(&now_utc, &utcTime); // convert to UTC struct tm (ignores TZ)

    // Now utcTime contains pure UTC components (no EST offset)

    char buf[80];
    strftime(buf, sizeof(buf), "  UTC Time: %Y-%m-%d %H:%M:%S UTC", &utcTime);
    DBG_PRINTLN(buf);
#endif
}

// ============ MIDNIGHT RESET ============
void setToMidnightForTesting()
{
    // Debug function to simulate midnight rollover by setting last_midnight_reset_date to yesterday

    pdata.last_midnight_reset_timestamp = getUtcTime(); // Subtract 24 hours in seconds
    pdata.last_midnight_reset_timestamp -= 86400;       // Subtract 24 hours in seconds
    savePersistentData();
    DBG_PRINTLN("[TEST] Simulated midnight rollover by setting last_midnight_reset_date to yesterday");
    printLocalTimestampTime("last_midnight_reset_date", pdata.last_midnight_reset_timestamp);
}

bool isNewLocalDay()
{
    // Returns true if the current local date is different from lastResetDate (YYYYMMDD)
    // theory is to look for a current date that is different from the last reset date, which would indicate that we've rolled over to a new day and need to do midnight housekeeping
    // getLocalTime() returns struct tm in local time
    struct tm lt = getLocalTime_Tm();
    char today[9];
    bool houseKeepingPerformed = false;

    struct tm local_midnight_reset_timestamp;
    time_t last_midnight_yday = pdata.last_midnight_reset_timestamp;
    localtime_r(&last_midnight_yday, &local_midnight_reset_timestamp); // applies TZ rukes to convert UTC reset to local time

    // design question: should  pdata.last_midnight_reset_timestamp be reset to compile time at compile time??
    // to ensure it is always initialized to a valid value that will trigger a reset on first run, and that we can rely on it being valid for logic that depends on it? If we do this, then we can simplify the logic in isNewLocalDay() to just compare the current date to the last reset date without needing to worry about it being uninitialized or invalid. We would just need to make sure to update it to the current date after performing the midnight reset housekeeping. This would be more robust than relying on it being initialized to 0 and having logic to handle that case, which could potentially lead to bugs if not handled correctly. It would also make the logic in isNewLocalDay() cleaner and easier to understand, since we would be able to assume that last_midnight_reset_timestamp is always valid and represents a real date.
    // the down side to this is that every time we compile, it will reset the last_midnight_reset_timestamp to the compile time, which could potentially cause issues if we are frequently compiling and testing, since it would trigger a midnight reset every time we upload new code. However, this could be mitigated by only setting it to the compile time if it is currently 0 (indicating that it has never been set before), rather than resetting it on every compile. This way, we would still ensure that it is initialized to a valid value on first run, but we would not overwrite it on subsequent compiles after it has already been set. Overall, I think this approach would be more robust and less error-prone than relying on it being initialized to 0 and having logic to handle that case in isNewLocalDay(), so I would recommend implementing this change in the persistence layer initialization logic.
    // ok, let's try setting last_midnight_reset_timestamp to the compile time if it is currently 0 in the persistence initialization logic and see how it goes. This should simplify the logic in isNewLocalDay() and make it more robust, while also ensuring that we have a valid timestamp to work with for the midnight reset logic.
    // the issue to watch is the midnight reset will reset the pills_taken_today_count to 0, so if we are frequently compiling and testing, we may want to consider adding a debug function to set last_midnight_reset_timestamp to a specific value for testing purposes, rather than relying on it being reset to the compile time on every compile. This way, we can have more control over when the midnight reset is triggered during testing, and we can avoid unintentionally resetting the pills_taken_today_count when we are just trying to test other functionality. We could implement this debug function to allow us to set last_midnight_reset_timestamp to any desired value, such as yesterday's date, to simulate a midnight rollover and test the isNewLocalDay() logic without affecting our regular testing workflow.

    if (lt.tm_yday != local_midnight_reset_timestamp.tm_yday)
    {
        DBG_PRINTLN("[MIDNIGHT] isNewLocalDay: New local day detected, performed midnight housekeeping");

        persistent_data_dump("[Midnight] prior to housekeeping"); // Debug dump of PersistentData before midnight housekeeping

        houseKeepingPerformed = true;
        pdata.last_midnight_reset_timestamp = getUtcTime();
        pdata.pills_taken_today_count = 0;
        savePersistentData();
        persistent_data_dump("[Midnight] after housekeeping"); // Debug dump of PersistentData after midnight housekeeping
    }
    else
    {
        //DBG_PRINTLN("[TIME] isNewLocalDay: Same local day - no midnight housekeeping needed");
    }
    return houseKeepingPerformed;
}