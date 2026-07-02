#pragma once
#include <U8g2lib.h>

#if __has_include("secrets.h")
  #include "secrets.h"
  #define SECRETS_IS_TEMPLATE 0
#elif __has_include("secrets_template.h")
  #include "secrets_template.h"
  #define SECRETS_IS_TEMPLATE 1
  #warning "Using dummy secrets from secrets_template.h. Create include/secrets.h for full functionality."
#else
  #error "Missing secrets header. Add include/secrets_template.h (public) or include/secrets.h (private)."
#endif

static_assert(wifi_credentials_count > 0, "At least one WiFi credential is required.");


// ==== Version Name ----
//#define VERSION_NAME "Milestone 4-F"
// moved to platfoirmio.ini to avoid rebuilds on version change

// ===================== NTP/Time Sync Parameters =====================
#define NTP_REFRESH_INTERVAL_SECONDS         1 * 24 * 60 * 60  // once per day (RC oscillator drift makes weekly interval too long)
#define NTP_RETRY_BACKOFF_INTERVAL_SECONDS   3600 //once per hour
#define NTP_RETRY_COUNT_MAX                 5  // number of retries before backoff issociated with each retry attempt, reset on successful sync
#define TIMEZONE_RULE "EST5EDT,M3.2.0/2,M11.1.0/2" // Eastern Time with DST


// Pin definitions (shared)
#define PB_TOP_PIN    1
#define PB_LEFT_PIN   4
#define PB_RIGHT_PIN  7
#define PB_SELECT_PIN 5
// Hardware-confirmed pin assignments: PB_LEFT=4, PB_RIGHT=7, PB_SELECT=5
#define LED_BUILTIN   15

// Timing definitions (shared)
#define INACTIVITY_TIMEOUT_MS 60000 // 60 seconds inactivity timeout
#define DEEP_SLEEP_WAKE_TIMER_SECONDS 300 // 5-minute timer wake interval (midnight detection latency <= 5 min)
#define DEBOUNCE_TIME_MS 20
#define NAV_DEBOUNCE_MS 60       // Debounce window for nav buttons (PB_LEFT/RIGHT/SELECT)
#define NAV_RELEASE_DEBOUNCE_MS 20 // Stable release-high time before nav button re-arms
#define SHORT_PRESS_MIN_MS 60       // Minimum press duration for short-press detection
#define LONG_PRESS_THRESHOLD_MS 4000 // Long press > 4000ms
#define PB_TOP_WATCHDOG_TIMEOUT_MS 15000 // Watchdog timeout (3.75x long press threshold) - allows for sustained holds with microbounce
#define CONNECT_TIMEOUT_MS 5000     // WiFi connect timeout per AP. Increase when connecting takes longer.

// === Persistence Debug macros ===
#define PERSISTENCE_DEBUG 0 // Set to 1 to enable persistence debug printing

// === Test Data Preload (cold boot only) ===
#define PRELOAD_TEST_DATA_ENABLE 0 // Set to 1 to preload test data on cold boot for testing persistence without needing to log pill events or manipulate RTC state to trigger resets. Only applies on cold boot, not deep sleep wake.
#define PRELOAD_PILLS_TAKEN_TODAY_COUNT 0
#define PRELOAD_RX_PILLS_PER_DAY 3
#define PRELOAD_PILL_REMAINING_COUNT 270

//=== Deep Sleep Enable Macro ===
# define DEEP_SLEEP_ENABLE 1  //set to 1 to enable deep sleep, 0 to disable for testing without sleep

// === Battery Monitoring (Milestone 4) ===
#define BATTERY_TEST_MODE_ENABLE 0  // Set to 1 to enable continuous battery display updates for testing

// Battery ADC Configuration
#define ADC_PIN 9                   // GPIO 9 (ADC1_CHANNEL_8)
#define ADC_ATTEN ADC_ATTEN_DB_11   // 11dB attenuation (safe up to ~2500mV)
#define NUM_SAMPLES 32              // 32 samples for noise reduction

// Voltage Divider Configuration
#define Rtop 470000.0f              // Top resistor (ohms)
#define Rbottom 496500.0f           // Bottom resistor (ohms)
// Divider ratio = Rbottom / (Rtop + Rbottom) ≈ 0.5137

// ADC Calibration Adjustments
#define ADC_OFFSET_ADJUSTMENT -0.00600f    // Consistent offset adjustment
#define DIVIDER_ADJUSTMENT 1.067273f       // Voltage divider adjustment factor

// USB Detection Threshold
#define USB_THRESHOLD 4.5f          // Voltage above which USB is detected

// Battery SOC Mapping (20% steps)
#define VBAT_100_THRESHOLD 4.15f    // 100%
#define VBAT_80_THRESHOLD 4.05f     // 80%
#define VBAT_60_THRESHOLD 3.95f     // 60%
#define VBAT_40_THRESHOLD 3.85f     // 40%
#define VBAT_20_THRESHOLD 3.75f     // 20%
#define VBAT_10_THRESHOLD 3.65f     // 10%
#define VBAT_0_THRESHOLD 3.55f      // 0% (Critical)

// Error Detection Bounds
#define VBAT_MIN_VALID 2.5f         // Minimum valid battery voltage
#define VBAT_MAX_VALID 6.0f         // Maximum valid battery voltage
#define VBAT_ERROR_DISPLAY 9.99f    // Voltage displayed on error

// === LED Control Macro ===
#define LED_ENABLE 1 // Set to 1 to enable LED output; default OFF (rely on OLED for status)

// === Setup State Enum ===
enum SetupState {
    SETUP_STATE_SERIAL_WAIT = 0,
    SETUP_INITIALIZATION = 1,
    SETUP_STATE_WIFI_CONNECTING = 2,
    SETUP_STATE_NTP_SYNCING = 3,
    SETUP_STATE_READY = 4
};

// === Serial debug macros ===
// // Set to 1 to enable Serial prints and debug delays
#ifndef SERIAL_DEBUG_ENABLE
  #define SERIAL_DEBUG_ENABLE 1
#endif

#if defined(SERIAL_DEBUG_ENABLE) && (SERIAL_DEBUG_ENABLE)
  #define DBG_PRINT(x)    Serial.print(x)
  #define DBG_PRINTLN(x) Serial.println(x)
  #define DBG_PRINTF(...) Serial.printf(__VA_ARGS__)  //example of variadic macro for printf    
  //#define DBG_PRINTLN(...) Serial.println(__VA_ARGS__)  //example of variadic macro for println
  #define DBG_DELAY(ms)  delay(ms)
  #define DBG_FLUSH()    Serial.flush()
#else
  #define DBG_PRINT(x)
  #define DBG_PRINTLN(x)
  #define DBG_PRINTF(...)   
  #define DBG_DELAY(ms)
  #define DBG_FLUSH()
#endif




// Use the correct display type for your hardware
extern U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2;

// Add other global variables here as needed
extern uint32_t lastActivityTime;

// Setup status globals
extern volatile uint8_t g_current_setup_state;
extern bool g_setup_complete;

extern char ssid_buffer[33]; // Buffer to hold SSID for debug printing (32 chars + null terminator)

// Battery placeholders for Milestone 3 UI (RAM-backed, non-persistent)
extern float battery_voltage_v;
extern uint8_t battery_capacity_pct;
extern char battery_condition[16];

// Edit mode active flag - disables PB_TOP short/long press while any edit/menu screen is active
extern bool g_edit_mode_active;

// Edit session state (Milestone 4)
typedef enum : uint8_t {
    EDIT_NONE       = 0,
    EDIT_RX_MENU    = 1,  // Rx Edit Menu List (entry from primary screen 2)
    EDIT_RX_DATE    = 2,  // Rx Date editor
    EDIT_RX_COUNT   = 3,  // Rx dispensed count editor
    EDIT_RX_PPD     = 4,  // Rx pills/day editor
    EDIT_PILLS_MENU = 5,  // Pills Remaining Edit Menu (entry from primary screen 3)
    EDIT_PILLS_REM  = 6,  // Pills Remaining editor
    EDIT_NTP_MENU   = 7,  // NTP Options Menu (entry from primary screen 5, PB_RIGHT)
} EditState;

extern EditState g_edit_state;
