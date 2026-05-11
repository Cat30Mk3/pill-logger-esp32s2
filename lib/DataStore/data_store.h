
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <esp_sleep.h>
#include "globals.h"

// ===================== Milestone 1 TEST SUMMARY =====================
// 1. without deepSleep confirmed NVS init(), load(), increment, save() cycle works by calling persistence_test_increment() and dumping PersistentData before/after to verify changes persist across reboots
// 2. with deepSleep enabled, confirmed that persistence_save() is called before sleep and data is correctly loaded on wake by using the same test function and dumps as above, but with deep sleep enabled in globals.h
// 3. confirmed magic number failure causes reset to defaults by changing PERSISTENCE_MAGIC to a different value, then rebooting and verifying via dump that defaults are loaded instead of previous data

// ===================== INTEGRATION SUMMARY =====================
// Persistence Layer (Milestone 1):
// - persistence_init() is called early in boot (before using PersistentData)
// - persistence_save() is called before deep sleep to commit PersistentData to NVS
// - rtc_save() is called before deep sleep to update RTC fast memory
// - DataStore now loads/saves PersistentData via Persistence API, not raw globals
// - Debug test: Enable PERSISTENCE_DEBUG in Globals to test persistence

// ===================== PERSISTENT DATA IN NVS ===================== RTC FAST MEMORY STATE =====================

struct PersistentData
{
    uint32_t last_pill_taken_timestamp;
    uint16_t pills_taken_today_count;
    uint16_t pill_remaining_count;
    uint32_t pills_depleted_date;
    uint32_t last_midnight_reset_timestamp;
    uint32_t last_compile_timestamp;
    // Rx data
    uint32_t Rx_last_refill_date;
    uint16_t Rx_dispensed_pill_count;
    uint8_t Rx_pills_per_day;
    uint32_t Rx_next_refill_date;
};

// ===================== RTC FAST MEMORY STATE =====================
typedef struct __attribute__((packed))
{
    uint32_t magic;
    uint32_t last_unix;
    uint32_t last_tick;
    char last_wifi_ssid[32];
    int8_t last_wifi_rssi;
    uint32_t last_ntp_sync_timestamp;
    uint8_t last_ntp_sync_reason;
    uint8_t last_ntp_failure_cause;
    uint8_t ntp_retry_count;
    uint8_t ntp_backoff_count;
    uint32_t ntp_backoff_start_ms; // ntp_backoff_active = millis() - ntp_backoff_start_ms < NTP_RETRY_BACKOFF_INTERVAL_SECONDS
    bool ntp_backoff_active;       // ntp_backoff_active = millis() - ntp_backoff_start_ms < NTP_RETRY_BACKOFF_INTERVAL_SECONDS    // backoff is active if ntp_retry_count >= NTP_RETRY_COUNT_MAX and backoff period has not expired
    bool live_clock_synced;
    // Future: add more RTC-only state here
} RtcFastState;

extern RtcFastState rtc_fast_state;

extern PersistentData pdata;

// --- Persistence Layer API ---

// ===================== INTEGRATION SUMMARY =====================
// Persistence Layer (Milestone 1):
// - persistence_init() is called early in boot (before using PersistentData)
//   acts as persistence_load() by loading from NVS or setting defaults if invalid
// - persistence_save() is called before deep sleep to commit PersistentData to NVS
// - rtc_save() is called before deep sleep to update RTC fast memory
// - DataStore now loads/saves PersistentData via Persistence API, not raw globals
// - Debug test: Enable PERSISTENCE_DEBUG in Globals to test persistence

// --- Persistence Layer API ---
void persistence_init();
void persistence_save();
void rtc_load();
void rtc_save();
void persistence_debug_log();
#ifdef PERSISTENCE_DEBUG
void persistence_test_increment();
void persistent_data_dump(const char *title);
void rtc_dump(const char *title);
#endif

// --- DataStore Management ---
void initDataStore();
void savePersistentData();
void preloadPersistentTestDataIfEnabled(esp_sleep_wakeup_cause_t wakeCause);

// --- Defaults ---
void setDefaultPersistentData();