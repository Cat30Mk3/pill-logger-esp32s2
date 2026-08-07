#pragma once

// ============================================================
// RTCClock — DS3231 Hardware Abstraction
// ============================================================
// Provides the sole interface between the DS3231M RTC hardware
// and the rest of the firmware. All RTClib references are
// confined to rtc_clock.cpp; nothing outside this module
// needs to include RTClib.h.
//
// MANDATORY CALL ORDER (enforced by hardware, not software):
//   1. initDisplay()        — triggers Wire.begin(SDA=33, SCL=35)
//   2. rtc_clock_begin()    — initialises DS3231 over that I2C bus
//
// Calling rtc_clock_begin() before initDisplay() will cause an
// I2C hang because Wire has not been started.
// ============================================================

// --- Initialisation ---
// Initialise DS3231, verify battery-backed oscillator is running,
// and synchronise the ESP32 system clock from DS3231 time.
//
// MUST be called after initDisplay() — Wire.begin(33,35) must run first.
//
// If the DS3231 OSF flag is set (VCC loss or dead coin cell), displays
// "RTC:Needs Sync" on the OLED, loads DS3231 time anyway, and returns
// with rtc_clock_is_ok() == false. NTP sync will clear the OSF via adjust().
void rtc_clock_begin();

// --- DS3231 → ESP32 system clock ---
// Reads current UTC time from DS3231 and calls settimeofday()
// to restore the ESP32 system clock.
// Called automatically by rtc_clock_begin(); may also be called
// manually if a re-sync is required.
void rtc_clock_sync_system_from_rtc();

// --- ESP32 system clock → DS3231 ---
// Reads current UTC system time and writes it to the DS3231.
// Must be called ONLY after a successful NTP sync so the DS3231
// is corrected for any accumulated drift.
void rtc_clock_sync_rtc_from_system();

// --- Health query ---
// Returns true if the DS3231 was found running with a valid
// coin-cell battery at the last call to rtc_clock_begin().
// Returns false if lostPower() was true (battery dead / replaced).
bool rtc_clock_is_ok();
