// ============================================================
// RTCClock — DS3231 Hardware Abstraction
// ============================================================
// MANDATORY CALL ORDER:
//   1. initDisplay()      — Wire.begin(SDA=33, SCL=35) runs inside u8g2.begin()
//   2. rtc_clock_begin()  — DS3231 init over that I2C bus
//
// All RTClib usage is confined to this file.
// ============================================================

#include <Arduino.h>
#include <RTClib.h>
#include <sys/time.h>
#include "rtc_clock.h"
#include "globals.h"       // DBG_PRINT / DBG_PRINTLN / u8g2
#include "time_logic.h"    // getUtcTime()

// ============================================================
// Module-private state
// ============================================================

// Single DS3231 instance — not exposed outside this file.
static RTC_DS3231 _rtc;

// Set true by rtc_clock_begin() when DS3231 is found healthy.
// Set false when lostPower() is detected.
static bool _rtc_ok = false;

// ============================================================
// Public API
// ============================================================

// MUST be called after initDisplay() — Wire.begin(33,35) must run first.
void rtc_clock_begin()
{
    DBG_PRINTLN("[RTC] rtc_clock_begin() — initialising DS3231");

    if (!_rtc.begin())
    {
        // DS3231 not found on I2C bus at all (wiring fault / missing hardware).
        DBG_PRINTLN("[RTC] *** ERROR: DS3231 not found on I2C bus ***");
        _rtc_ok = false;

        // Display permanent alert — device cannot operate without a time source.
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.drawStr(4,  14, "DS3231 NOT FOUND");
        u8g2.drawStr(4,  28, "Check wiring");
        u8g2.sendBuffer();

        DBG_PRINTLN("[RTC] Hard stop — system halted");
        while (true) { delay(1000); }
    }

    if (_rtc.lostPower())
    {
        // OSF set — oscillator was stopped at some point (VCC loss or dead coin cell).
        // DS3231 coin-cell time is loaded anyway so NTP interval check fires correctly.
        DBG_PRINTLN("[RTC] *** WARNING: DS3231 lostPower() — OSF set, NTP sync required ***");
        _rtc_ok = false;

        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_ncenB08_tr);
        u8g2.drawStr(10, 14, "RTC:Needs Sync");
        u8g2.drawStr(10, 28, "Connect WiFi");
        u8g2.sendBuffer();

        rtc_clock_sync_system_from_rtc();
        return;
    }

    // DS3231 is present and running with a valid coin-cell backup.
    _rtc_ok = true;
    DBG_PRINTLN("[RTC] DS3231 healthy — syncing system clock from RTC");

    // Restore ESP32 system clock from DS3231 time.
    // This is the critical step that makes all time_logic functions
    // return correct times on every boot and every wake from deep sleep.
    rtc_clock_sync_system_from_rtc();
}

void rtc_clock_sync_system_from_rtc()
{
    // Read current UTC time from DS3231 and set the ESP32 system clock.
    // The DS3231 always stores UTC. DST / timezone conversion is handled
    // separately at display time by getLocalTime_Tm() in time_logic.cpp.
    DateTime now = _rtc.now();
    struct timeval tv;
    tv.tv_sec  = (time_t)now.unixtime();
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);

    DBG_PRINTF("[RTC] System clock set from DS3231: %04u-%02u-%02u %02u:%02u:%02u UTC\n",
               now.year(), now.month(), now.day(),
               now.hour(), now.minute(), now.second());
}

void rtc_clock_sync_rtc_from_system()
{
    // Write current ESP32 system clock (UTC) back to the DS3231.
    // Called only after a successful NTP sync to correct DS3231 drift.
    time_t t = (time_t)getUtcTime();
    _rtc.adjust(DateTime((uint32_t)t)); // adjust() clears the OSF bit as a side effect
    _rtc_ok = true;

    // Read back for confirmation log.
    DateTime confirm = _rtc.now();
    DBG_PRINTF("[RTC] DS3231 updated from NTP: %04u-%02u-%02u %02u:%02u:%02u UTC\n",
               confirm.year(), confirm.month(), confirm.day(),
               confirm.hour(), confirm.minute(), confirm.second());
}

bool rtc_clock_is_ok()
{
    return _rtc_ok;
}
