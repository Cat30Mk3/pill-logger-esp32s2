#include <Arduino.h>
#include "data_store.h"
#include "globals.h"
#include "esp_attr.h"
#include "esp_system.h"
#include "esp_sleep.h"
#include "time_logic.h"
#include <Preferences.h>

#define PERSISTENCE_NVS_NAMESPACE "pill_logger"
#define PERSISTENCE_NVS_KEY "persistent_data"
#define PERSISTENCE_MAGIC 0xA5A5A5A5 /// 0xA5A5A5A5
#define RTC_MAGIC 0x5A5A5A5A

// ============ GLOBAL STATE ============
PersistentData pdata = {0};
RtcFastState rtc_fast_state = {0};

// ============ DATA STORE INITIALIZATION ============
void initDataStore()
{
    persistence_init();
    rtc_load();

    // BUILD_UNIX_TIME is injected by PlatformIO at build time
    // a build flag in platforimio.ini defines BUILD_UNIX_TIME as the current unix timestamp at build time,
    // which we can use to set compile timestamps in PersistentData for validation and midnight reset logic

    time_t compile_time_t = (time_t)BUILD_UNIX_TIME;

    pdata.last_compile_timestamp = compile_time_t;

    if (pdata.last_midnight_reset_timestamp == 0)
    {
        pdata.last_midnight_reset_timestamp = compile_time_t;
        persistence_save();
    }
}

void example_compile_date_time()
{
    persistence_init();
    rtc_load();

    // BUILD_UNIX_TIME is injected by PlatformIO at build time
    time_t compile_time_t = (time_t)BUILD_UNIX_TIME;

    pdata.last_compile_timestamp = compile_time_t;
    pdata.last_midnight_reset_timestamp = compile_time_t;
}

// ============ NVS MANAGEMENT ============
void savePersistentData()
{
    persistence_save();
}

void preloadPersistentTestDataIfEnabled(esp_sleep_wakeup_cause_t wakeCause)
{
#if PRELOAD_TEST_DATA_ENABLE
    // Only seed on cold boot; do not overwrite data on deep-sleep wake paths.
    if (wakeCause == ESP_SLEEP_WAKEUP_UNDEFINED)
    {
        pdata.pills_taken_today_count = PRELOAD_PILLS_TAKEN_TODAY_COUNT;
        pdata.pill_remaining_count = PRELOAD_PILL_REMAINING_COUNT;
        pdata.Rx_pills_per_day = PRELOAD_RX_PILLS_PER_DAY;

        if (pdata.Rx_pills_per_day > 0)
        {
            uint32_t days_to_depletion =
                (pdata.pill_remaining_count + pdata.Rx_pills_per_day - 1) / pdata.Rx_pills_per_day;
            pdata.pills_depleted_date = getUtcTime() + (days_to_depletion * 86400UL);
        }

        savePersistentData();
        persistent_data_dump("After preload test seed");
    }
#else
    (void)wakeCause;
#endif
}

// ============ DEFAULTS ============
void setDefaultPersistentData()
{
    // Use compile date/time for all timestamps
    struct tm tm = {0};
    strptime(__DATE__ " " __TIME__, "%b %d %Y %H:%M:%S", &tm);
    time_t build_time = mktime(&tm);
    pdata.last_compile_timestamp = build_time;
    pdata.last_pill_taken_timestamp = build_time;
    pdata.pills_taken_today_count = 0;
    pdata.pill_remaining_count = 0;
    pdata.pills_depleted_date = 0;
    pdata.last_midnight_reset_timestamp = build_time;
    //  pdata.last_ntp_sync_timestamp REMOVED (now in RTC Fast Memory)

    pdata.Rx_last_refill_date = build_time;
    pdata.Rx_dispensed_pill_count = 0;
    pdata.Rx_pills_per_day = 0;
    pdata.Rx_next_refill_date = build_time;
}

// ===================== PERSISTENCE LAYER IMPLEMENTATION =====================

static Preferences prefs;

typedef struct
{
    uint32_t magic;
    PersistentData data;
} NvsBlob;

static NvsBlob nvs_blob;

void persistence_init()
{
    prefs.begin(PERSISTENCE_NVS_NAMESPACE, false);
    size_t len = prefs.getBytes(PERSISTENCE_NVS_KEY, &nvs_blob, sizeof(nvs_blob));
    if (len != sizeof(nvs_blob) || nvs_blob.magic != PERSISTENCE_MAGIC)
    {
        setDefaultPersistentData();
        nvs_blob.magic = PERSISTENCE_MAGIC;
        nvs_blob.data = pdata;
        prefs.putBytes(PERSISTENCE_NVS_KEY, &nvs_blob, sizeof(nvs_blob));
    }
    else
    {
        pdata = nvs_blob.data;
    }
}

void persistence_save()
{
    nvs_blob.magic = PERSISTENCE_MAGIC;
    nvs_blob.data = pdata;
    prefs.putBytes(PERSISTENCE_NVS_KEY, &nvs_blob, sizeof(nvs_blob));
}

RTC_FAST_ATTR static RtcFastState rtc_blob;

void rtc_load()
{
    if (rtc_blob.magic == RTC_MAGIC)
    {
        rtc_fast_state = rtc_blob;
        return;
    }

    // First boot (or invalid RTC content): initialize a known-good runtime state
    // and mirror it into RTC fast memory.
    rtc_fast_state.magic = RTC_MAGIC;
    rtc_fast_state.last_unix = (uint32_t)(__TIME__[0]) + 1750000000UL; // crude compile-time stub
    rtc_fast_state.last_tick = (uint32_t)esp_timer_get_time() / 1000;
    rtc_fast_state.last_ntp_sync_timestamp = 0;
    rtc_fast_state.last_ntp_failure_cause = 0;
    rtc_fast_state.ntp_retry_count = 0;
    rtc_fast_state.ntp_backoff_count = 0;
    rtc_fast_state.ntp_backoff_start_ms = 0;
    rtc_fast_state.ntp_backoff_active = false;
    rtc_fast_state.live_clock_synced = false;

    rtc_blob = rtc_fast_state;
}

void rtc_save()
{
    rtc_fast_state.magic = RTC_MAGIC;
    rtc_fast_state.last_tick = (uint32_t)esp_timer_get_time() / 1000;

    // Persist runtime RTC state into RTC fast memory.
    rtc_blob = rtc_fast_state;
}

#ifdef PERSISTENCE_DEBUG
#include <time.h>

// Increment all count fields for test

void persistence_test_increment()
{
    pdata.pills_taken_today_count++;
    pdata.pill_remaining_count++;
    pdata.Rx_dispensed_pill_count++;
    pdata.Rx_pills_per_day++;
    // Increment all timestamp fields by 60 seconds
    pdata.last_pill_taken_timestamp += 60;
    pdata.pills_depleted_date += 60;
    pdata.Rx_last_refill_date += 60;
    pdata.Rx_next_refill_date += 60;
}

// Print PersistentData in readable format
void persistent_data_dump(const char *title)
{
    DBG_PRINTLN("--------------------------");
    DBG_PRINT("[Data] NVS PersistentData Dump: ");
    DBG_PRINTLN(title);
    char buf[32];

    // Print timestamps as readable dates
    time_t t;
    struct tm local_tm; // declare ONCE

    t = pdata.last_compile_timestamp;
    local_tm = getLocalTimestampTime(t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local_tm);
    DBG_PRINT("[Data] last_compile_timestamp: ");
    DBG_PRINTLN(buf);

    t = pdata.last_pill_taken_timestamp;
    local_tm = getLocalTimestampTime(t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local_tm);
    DBG_PRINT("[Data] last_pill_taken_timestamp: ");
    DBG_PRINTLN(buf);

    DBG_PRINT("[Data] pills_taken_today_count: ");
    DBG_PRINTLN(pdata.pills_taken_today_count);
    DBG_PRINT("[Data] pill_remaining_count: ");
    DBG_PRINTLN(pdata.pill_remaining_count);

    t = pdata.pills_depleted_date;
    local_tm = getLocalTimestampTime(t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local_tm);
    DBG_PRINT("[Data] pills_depleted_date: ");
    DBG_PRINTLN(buf);

    t = pdata.last_midnight_reset_timestamp;
    local_tm = getLocalTimestampTime(t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local_tm);
    DBG_PRINT("[Data] last_midnight_reset_timestamp: ");
    DBG_PRINTLN(buf);

    t = pdata.Rx_last_refill_date;
    local_tm = getLocalTimestampTime(t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local_tm);
    DBG_PRINT("[Data] Rx_last_refill_date: ");
    DBG_PRINTLN(buf);

    DBG_PRINT("[Data] Rx_dispensed_pill_count: ");
    DBG_PRINTLN(pdata.Rx_dispensed_pill_count);
    DBG_PRINT("[Data] Rx_pills_per_day: ");
    DBG_PRINTLN(pdata.Rx_pills_per_day);

    t = pdata.Rx_next_refill_date;
    local_tm = getLocalTimestampTime(t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &local_tm);
    DBG_PRINT("[Data] Rx_next_refill_date: ");
    DBG_PRINTLN(buf);

    DBG_PRINTLN("--------------------------");
}

// Print RTC Fast Memory struct in readable format
void rtc_dump(const char *title)
{
    DBG_PRINTLN("--------------------------");
    DBG_PRINT("[Data] RTC Fast Memory Dump: ");
    DBG_PRINTLN(title);
    char buf[32];
    time_t t;

    DBG_PRINT("[Data] magic: 0x");
    char hexbuf[12];
    sprintf(hexbuf, "%08lX", (unsigned long)rtc_fast_state.magic);
    DBG_PRINTLN(hexbuf);

    t = rtc_fast_state.last_unix;
    struct tm tm = getLocalTimestampTime(t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    DBG_PRINT("[Data] last_unix: ");
    DBG_PRINTLN(buf);

    DBG_PRINT("[Data] last_tick: ");
    DBG_PRINTLN((unsigned long)rtc_fast_state.last_tick);

    DBG_PRINT("[Data] live_clock_synced: ");
    DBG_PRINTLN(rtc_fast_state.live_clock_synced ? "true" : "false");

    t = rtc_fast_state.last_ntp_sync_timestamp;
    tm = getLocalTimestampTime(t);
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &tm);
    DBG_PRINT("[Data] last_ntp_sync_timestamp: ");
    DBG_PRINTLN(buf);

    DBG_PRINT("[Data] ntp_retry_count: ");
    DBG_PRINTLN((unsigned long)rtc_fast_state.ntp_retry_count);

    DBG_PRINT("[Data] last_ntp_sync_reason: ");
    DBG_PRINTLN((unsigned long)rtc_fast_state.last_ntp_sync_reason);

    DBG_PRINT("[Data] last_ntp_failure_cause: ");
    DBG_PRINTLN((unsigned long)rtc_fast_state.last_ntp_failure_cause);

    DBG_PRINT("[Data] ntp_backoff_start_ms: ");
    DBG_PRINTLN((unsigned long)rtc_fast_state.ntp_backoff_start_ms);

    DBG_PRINT("[Data] ntp_backoff_active: ");
    DBG_PRINTLN(rtc_fast_state.ntp_backoff_active ? "true" : "false");

    DBG_PRINTLN("--------------------------");
}

#endif