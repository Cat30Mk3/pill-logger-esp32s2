
// ===================== INTEGRATION SUMMARY =====================
// Time/NTP Logic (Milestone 2, Part 1):
// - All NTP/time config macros and WiFi credentials are in Globals
// - All runtime state (NTP sync, retry, last sync, live clock) is in RTC Fast State (see DataStore)
// - Call initWiFi() early in boot to set up SSID/PWD pairsfor WiFiMulti
// - Call syncTimeIfNeeded() everytime setup is called to ensure time is synced on boot and wake from deep sleep as needed
// - Use getUtcTime() for current UTC timestamp (POSIX time_t) - this is ESP System Time which is set by NTP and is independent of timezone 
// - Use getLocalTime() to get struct tm for display (local time) with time zone applied
// - Timezone is set via TZ environment variable in getLocalTime() - this is required because TZ env does not persist across deep sleep, so we set it on each call to getLocalTime() to ensure correct local time is always returned
// - NTP retry logic with backoff is implemented in ntp_retry_logic() and ntp_sync_attempt() - this is called by syncTimeIfNeeded() when needed
// - isNewLocalDay() compares current local date to last reset date to determine if midnight housekeeping is needed
// - Debug/test hooks available to force NTP sync and simulate midnight rollover    
// - Example usage:
//      tm local_tm = getLocalTime();
//      DBG_PRINT("Local time: ");
//      DBG_PRINT(local_tm.tm_year+1900); DBG_PRINT("-");
//      DBG_PRINT(local_tm.tm_mon+1); DBG_PRINT("-");
//      DBG_PRINT(local_tm.tm_mday); DBG_PRINT(" ");
//      DBG_PRINT(local_tm.tm_hour); DBG_PRINT(":");
//      DBG_PRINT(local_tm.tm_min); DBG_PRINT(":");
//      DBG_PRINTLN(local_tm.tm_sec);
//      DBG_PRINT("UTC timestamp: "); DBG_PRINTLN(getUtcTime());
// - Use isNewLocalDay() to detect midnight rollover for daily housekeeping
// - NTP retry/backoff logic is automatic; debug/test hooks available (ntp_debug_force_sync, ntp_debug_force_midnight)
// - No new libraries; only uses Arduino, WiFi, and standard ESP32/Arduino APIs


//============= Tests =============
//1. Basic NTP Sync Test: Force NTP sync on startup and print local and UTC time to verify correct sync and timezone handling.
//   - confirmed wigi Multi capability is functioning with these exceptions:
//    (1) to disable SSID and force to strongest enabled, disrupt SSID name - not password
//    (2) a bad password for an active SSID will cause a disconnect and stop multi SSID scanning, even if other pairs in list are valid.
//    (3) to enable hotspot visibility on Johns iPhone 17, must set Max Compatibility ON,
//      
// 2. NTP Retry and Backoff Test: Use debug function to force NTP sync failure and verify that retry logic attempts retries and then enters backoff after max retries, and that it attempts sync again after backoff period expires. Check RTC state variables to confirm correct behavior.
//    - confirmed that NTP retry logic is working as expected, with retries occurring at the defined interval and backoff activating after max retries, and that it attempts sync again after backoff period expires. RTC state variables are updating correctly to reflect retry count, backoff state, and last failure cause.
//    - still need to confirm - periodic sync is working as expected based on time since last sync, and that it correctly identifies when sync is not needed because clock is already synced and refresh interval has not expired. Also need to confirm that it correctly identifies invalid system time and forces sync in that case as well. Will test this by manipulating system time to be invalid and verifying that sync is triggered, and by manipulating last sync timestamp to simulate refresh interval expiration and verifying that sync is triggered in that case as well.    
//    - still need to confirm deepSleep wakeup logic is working as expected, with it correctly assessing whether NTP sync is needed based on wakeup cause and time since last sync, and that it does not attempt sync if clock is already synced and refresh interval has not expired. Will test this by simulating deep sleep wakeup with different wakeup causes and manipulating last sync timestamp to simulate different scenarios (sync needed vs not needed) and verifying that it behaves as expected in each case.
//
// 3. Midnight Rollover Test: Use debug function to simulate midnight rollover and verify that isNewLocalDay() correctly detects the new day and updates last reset date accordingly.
//    - confirmed - i forced last_midnight_reset_timestamp to yesterday and verified that isNewLocalDay() detects the new day and updates last_midnight_reset_timestamp to the current date and resets pills_taken_today_count. Will also verify that it does not perform housekeeping if the date has not changed. 




#pragma once
#include <stdint.h>

// ===================== NTP Failure Cause Codes =====================
typedef enum {
    NTP_OK = 0,
    NTP_WIFI_FAIL = 1,
    NTP_DNS_FAIL = 2,
    NTP_UNREACHABLE = 3,
    NTP_TIMEOUT = 4,
    NTP_INVALID = 5
} NtpFailureCause;

// 10-char string table for display
static const char* ntp_failure_cause_str[] = {
    "OK",
    "WiFi_DN",
    "DNS_DN",
    "NTP_DN",
    "NTP_TO",
    "NTP_INV"
};

// ===================== NTP Sync Logic =====================
void ntp_retry_logic();
boolean ntp_sync_attempt();

// --- wiFi Initialization ---
void initWiFi();

// --- NTP Sync ---
void syncTimeIfNeeded(bool force);
void in_setup_assess_ntp_sync_needed(esp_sleep_wakeup_cause_t cause);
void in_loop_assess_ntp_sync_needed();

// --- UTC / Local Conversion ---
tm getLocalTime_Tm();
tm getUtcTime_Tm();
tm getLocalTimestampTime(time_t timestamp);
uint32_t getUtcTime();
void printLocalTime();
void printUTCTime();
void printLocalTimestampTime(const char* TimeStampName, time_t timestamp);

// --- Midnight Reset ---
bool isNewLocalDay();
void setToMidnightForTesting();

// --- Manual Sync Helpers (used by display_ui manual NTP sync) ---
bool wifi_connect_for_ntp();  // WiFi connection phase only
bool ntp_sync_only();         // NTP time sync phase only (assumes WiFi already connected)