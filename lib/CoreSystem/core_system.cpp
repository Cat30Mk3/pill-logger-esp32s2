#include <Arduino.h>
#include "core_system.h"
#include <esp_sleep.h>
#include <esp_timer.h>
#include "globals.h"
#include "data_store.h"
#include "time_logic.h"
#include "display_ui.h"


// ============ GLOBAL STATE ============
RtcState rtcState = {0};

// ============ BUTTON STATE ============
// ISRs just record edge times - all logic is in processButtonEvents()
volatile uint32_t pb_falling_time = 0; // Time when button pressed (FALLING)
volatile uint32_t pb_rising_time = 0;  // Time when button released (RISING)
volatile bool pb_falling_detected = false;
volatile bool pb_rising_detected = false;

// Press detection results (non-static so they can be accessed from main.cpp)
volatile bool pb_short_press_detected = false;
volatile bool pb_long_press_detected = false;

// ============ PB_TOP TIMER-BASED STATE MACHINE ============
struct PbTopTimerState {
    enum Phase : uint8_t {
        IDLE,           // Waiting for button press
        DEBOUNCE,       // Waiting for debounce period
        MEASURING,      // Measuring hold duration
        DETECTED,       // Press detected, ready to report
        LATCHING        // Button released, waiting for stable HIGH to re-arm
    } phase;
    
    uint64_t phase_start_us;            // When current phase started
    uint32_t press_fall_time_ms;        // Time when button pressed (FALLING edge)
    uint32_t press_rise_time_ms;        // Time when button released (RISING edge)
    uint32_t hold_duration_ms;          // Duration button was held
    
    volatile bool short_press_ready;    // Short press detected, ready to consume
    uint32_t last_short_press_ms;       // Duration of last short press (for reporting)
    
    volatile bool long_press_ready;     // Long press detected, ready to consume
    uint32_t last_long_press_ms;        // Duration of last long press (for reporting)
    
    esp_timer_handle_t timer_handle;    // esp_timer handle
    bool timer_active;                  // Is timer currently running?
    
    volatile bool pb_top_enabled;       // Can button respond?
};

static PbTopTimerState pb_top = {
    .phase = PbTopTimerState::IDLE,
    .phase_start_us = 0,
    .press_fall_time_ms = 0,
    .press_rise_time_ms = 0,
    .hold_duration_ms = 0,
    .short_press_ready = false,
    .last_short_press_ms = 0,
    .long_press_ready = false,
    .last_long_press_ms = 0,
    .timer_handle = NULL,
    .timer_active = false,
    .pb_top_enabled = false
};

// Constants (milliseconds) - using globals.h definitions where available
// Additional PB_TOP-specific constants not in globals.h:
const uint32_t PB_TOP_STABLE_RELEASE_MS = 20;      // Debounce on release

// ============ PB_TOP TIMER CALLBACK (runs independently of main loop) ============
static void IRAM_ATTR pb_top_timer_callback(void *arg)
{
    uint64_t now_us = esp_timer_get_time();
    uint64_t elapsed_us = now_us - pb_top.phase_start_us;
    uint32_t elapsed_ms = elapsed_us / 1000;
    
    switch (pb_top.phase) {
        
    case PbTopTimerState::DEBOUNCE:
        // Wait for debounce period to expire
        if (elapsed_ms >= DEBOUNCE_TIME_MS) {
            pb_top.phase = PbTopTimerState::MEASURING;
            pb_top.phase_start_us = now_us;
            DBG_PRINTF("[PB_TOP] DEBOUNCE passed (%lu ms) → MEASURING\n", elapsed_ms);
        }
        break;
        
    case PbTopTimerState::MEASURING:
        // Watching hold duration, waiting for RISING edge (will be handled in ISR)
        // But watchdog if stuck too long
        if (elapsed_ms > PB_TOP_WATCHDOG_TIMEOUT_MS) {
            DBG_PRINTF("[PB_TOP WATCHDOG] Stuck in MEASURING for %lu ms, resetting to IDLE\n", elapsed_ms);
            pb_top.phase = PbTopTimerState::IDLE;
            pb_top.phase_start_us = 0;
        }
        break;
        
    case PbTopTimerState::LATCHING:
        // Waiting for stable HIGH release before re-arming
        if (digitalRead(PB_TOP_PIN) == HIGH) {
            if (elapsed_ms >= PB_TOP_STABLE_RELEASE_MS) {
                pb_top.phase = PbTopTimerState::IDLE;
                pb_top.phase_start_us = 0;
                DBG_PRINTF("[PB_TOP] LATCHING stable HIGH (%lu ms) → IDLE (re-armed)\n", elapsed_ms);
            }
        } else {
            // Button pressed again during latching - restart
            pb_top.phase_start_us = now_us;
            DBG_PRINTLN("[PB_TOP] LATCHING: button pressed again, resetting latching timer");
        }
        break;
        
    default:
        break;
    }
}

// ============ PB_TOP ISR (called on every edge: FALLING or RISING) ============
static void IRAM_ATTR pb_top_isr_handler()
{
    if (!pb_top.pb_top_enabled) {
        return;  // Button disabled, ignore
    }
    
    int pin_state = digitalRead(PB_TOP_PIN);
    uint64_t now_us = esp_timer_get_time();
    uint32_t now_ms = millis();
    
    if (pin_state == LOW) {
        // FALLING edge (button pressed)
        if (pb_top.phase == PbTopTimerState::IDLE) {
            pb_top.press_fall_time_ms = now_ms;
            pb_top.phase = PbTopTimerState::DEBOUNCE;
            pb_top.phase_start_us = now_us;
            DBG_PRINTF("[PB_TOP ISR] FALLING edge detected at %lu ms → DEBOUNCE\n", now_ms);
        }
    } else {
        // RISING edge (button released)
        if (pb_top.phase == PbTopTimerState::MEASURING) {
            pb_top.press_rise_time_ms = now_ms;
            pb_top.hold_duration_ms = pb_top.press_rise_time_ms - pb_top.press_fall_time_ms;
            DBG_PRINTF("[PB_TOP ISR] RISING edge at %lu ms, hold duration: %lu ms\n", now_ms, pb_top.hold_duration_ms);
            
            // Sanity check hold duration
            if (pb_top.hold_duration_ms < DEBOUNCE_TIME_MS || pb_top.hold_duration_ms > 10000) {
                DBG_PRINTF("[PB_TOP ISR] Hold duration OUT OF RANGE (%lu ms), rejecting → LATCHING\n", pb_top.hold_duration_ms);
                pb_top.phase = PbTopTimerState::LATCHING;
            } else {
                // Valid hold duration - determine press type
                if (pb_top.hold_duration_ms >= LONG_PRESS_THRESHOLD_MS) {
                    pb_top.long_press_ready = true;
                    pb_top.last_long_press_ms = pb_top.hold_duration_ms;
                    DBG_PRINTF("[PB_TOP ISR] *** LONG PRESS DETECTED (%lu ms >= %lu ms) ***\n", pb_top.hold_duration_ms, (uint32_t)LONG_PRESS_THRESHOLD_MS);
                } else if (pb_top.hold_duration_ms >= SHORT_PRESS_MIN_MS) {
                    pb_top.short_press_ready = true;
                    pb_top.last_short_press_ms = pb_top.hold_duration_ms;
                    DBG_PRINTF("[PB_TOP ISR] Short press detected (%lu ms)\n", pb_top.hold_duration_ms);
                }
                pb_top.phase = PbTopTimerState::LATCHING;
            }
            pb_top.phase_start_us = now_us;
        } else {
            DBG_PRINTF("[PB_TOP ISR] RISING edge but phase != MEASURING (phase=%u), ignoring\n", (unsigned)pb_top.phase);
        }
    }
}

// ============ PB_TOP PUBLIC API ============
void pb_top_init()
{
    DBG_PRINTLN("[PB_TOP INIT] Starting button handler initialization");
    
    pinMode(PB_TOP_PIN, INPUT_PULLUP);
    DBG_PRINTF("[PB_TOP INIT] Set pin %d to INPUT_PULLUP\n", PB_TOP_PIN);
    
    // Create and start esp_timer for state machine
    const esp_timer_create_args_t timer_args = {
        .callback = pb_top_timer_callback,
        .arg = nullptr,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "pb_top_sm"
    };
    
    if (esp_timer_create(&timer_args, &pb_top.timer_handle) == ESP_OK) {
        pb_top.timer_active = true;
        esp_timer_start_periodic(pb_top.timer_handle, 5000);  // 5ms callback
        DBG_PRINTLN("[PB_TOP INIT] Timer created and started (5ms period)");
    } else {
        DBG_PRINTLN("[PB_TOP INIT] *** ERROR: Failed to create timer ***");
    }
    
    // Attach ISR to handle edge detection
    attachInterrupt(PB_TOP_PIN, pb_top_isr_handler, CHANGE);
    DBG_PRINTLN("[PB_TOP INIT] ISR attached to CHANGE edges");
    
    // Initially disabled (will be enabled after setup complete)
    pb_top.pb_top_enabled = false;
    DBG_PRINTLN("[PB_TOP INIT] Button detection initialized but DISABLED (will enable after setup)");
}

void pb_top_enable()
{
    pb_top.pb_top_enabled = true;
    DBG_PRINTLN("[PB_TOP] *** BUTTON DETECTION ENABLED ***");
}

void pb_top_disable()
{
    pb_top.pb_top_enabled = false;
    pb_top.short_press_ready = false;
    pb_top.long_press_ready = false;
    pb_top.phase = PbTopTimerState::IDLE;
    DBG_PRINTLN("[PB_TOP] Button detection disabled");
}

bool pb_top_is_short_press()
{
    if (pb_top.short_press_ready) {
        pb_top.short_press_ready = false;
        return true;
    }
    return false;
}

bool pb_top_is_long_press()
{
    if (pb_top.long_press_ready) {
        pb_top.long_press_ready = false;
        return true;
    }
    return false;
}

// ============ NAV BUTTON STATE ============
// Each nav press is latched once, then blocked until a stable release is seen.
// This eliminates bounce multi-counting and enforces one event per press-release cycle.
struct NavBtnState {
    uint8_t           pin;
    volatile uint32_t last_falling_ms;
    volatile uint32_t release_high_since_ms;
    volatile bool     press_detected;
    volatile bool     rearmed;
};

static NavBtnState navLeft   = {PB_LEFT_PIN,   0, 0, false, true};
static NavBtnState navRight  = {PB_RIGHT_PIN,  0, 0, false, true};
static NavBtnState navSelect = {PB_SELECT_PIN, 0, 0, false, true};

static void IRAM_ATTR onNavFalling(NavBtnState &btn) {
    uint32_t now = millis();
    if (!btn.rearmed) {
        return;
    }
    if ((now - btn.last_falling_ms) < NAV_DEBOUNCE_MS) {
        return;
    }
    btn.last_falling_ms       = now;
    btn.release_high_since_ms = 0;
    btn.press_detected        = true;
    btn.rearmed               = false;
}

// ============ NAV BUTTON ISR HANDLERS ============
void IRAM_ATTR pbLeftIsrHandler() {
    onNavFalling(navLeft);
}

void IRAM_ATTR pbRightIsrHandler() {
    onNavFalling(navRight);
}

void IRAM_ATTR pbSelectIsrHandler() {
    onNavFalling(navSelect);
}

// ============ NAV BUTTON EVENT CONSUMER ============
// Rearm only after stable HIGH release so one physical press creates one event.
static void processOneNavBtn(NavBtnState &btn) {
    if (!btn.rearmed) {
        if (digitalRead(btn.pin) == HIGH) {
            uint32_t now_ms = millis();
            if (btn.release_high_since_ms == 0) {
                btn.release_high_since_ms = now_ms;
            } else if ((now_ms - btn.release_high_since_ms) >= NAV_RELEASE_DEBOUNCE_MS) {
                btn.rearmed = true;
            }
        } else {
            btn.release_high_since_ms = 0;
        }
    }

    if (btn.press_detected) {
        lastActivityTime = millis();
    }
}

void processNavButtonEvents() {
    processOneNavBtn(navLeft);
    processOneNavBtn(navRight);
    processOneNavBtn(navSelect);
}

bool isLeftPress() {
    if (navLeft.press_detected) { navLeft.press_detected = false; return true; }
    return false;
}

bool isRightPress() {
    if (navRight.press_detected) { navRight.press_detected = false; return true; }
    return false;
}

bool isSelectPress() {
    if (navSelect.press_detected) { navSelect.press_detected = false; return true; }
    return false;
}

void clearAllNavPressFlags() {
    navLeft.press_detected   = false;
    navRight.press_detected  = false;
    navSelect.press_detected = false;
}


// ============ INITIALIZATION ============
void initCoreSystem()
{
    // PB_TOP: short/long press for pill logging (timer-based, independent of main loop)
    pb_top_init();

    // Nav buttons: PB_LEFT, PB_RIGHT, PB_SELECT for MUI editing (Milestone 4)
    pinMode(PB_LEFT_PIN,   INPUT_PULLUP);
    attachInterrupt(PB_LEFT_PIN,   pbLeftIsrHandler,   FALLING);
    pinMode(PB_RIGHT_PIN,  INPUT_PULLUP);
    attachInterrupt(PB_RIGHT_PIN,  pbRightIsrHandler,  FALLING);
    pinMode(PB_SELECT_PIN, INPUT_PULLUP);
    attachInterrupt(PB_SELECT_PIN, pbSelectIsrHandler, FALLING);

    loadRtcState();
}

// ============ RTC STATE MANAGEMENT ============
void loadRtcState()
{
    if (rtcState.magic != RTC_MAGIC)
    {
        rtcState.magic = RTC_MAGIC;
        rtcState.screen_index = 0;
        rtcState.last_wake_timestamp = 0; // Initialize to 0; real logic added later
        rtcState.flags = 0;
    }
}

void saveRtcState()
{
    rtc_save();
}

// ============ WAKE HANDLING ============

// ============ DEEP SLEEP ============
void enterDeepSleep()
{
    DBG_PRINTLN("[SLEEP] Entering deep sleep...");
    printLocalTime();

    saveRtcState();
    esp_sleep_enable_ext0_wakeup((gpio_num_t)PB_TOP_PIN, 0); // 0 = wake on LOW
    esp_sleep_enable_timer_wakeup((uint64_t)DEEP_SLEEP_WAKE_TIMER_SECONDS * 1000000ULL);
#if LED_ENABLE
    digitalWrite(LED_BUILTIN, LOW);
#endif
    delay(100);
    esp_deep_sleep_start();
}

// ============ PILL TAKEN HANDLER ============
void processPillTakenLongPress()
{
    persistent_data_dump("Before processing pill taken");

    if (pdata.pill_remaining_count == 0)
    {
        showConfirmationPopup("No pills remaining", "Press button");
        persistent_data_dump("No pills remaining - no changes applied");
        return;
    }

    pdata.pills_taken_today_count++;
    pdata.pill_remaining_count--;
    pdata.last_pill_taken_timestamp = getUtcTime();

    if (pdata.Rx_pills_per_day > 0)
    {
        uint32_t days_to_depletion =
            (pdata.pill_remaining_count + pdata.Rx_pills_per_day - 1) / pdata.Rx_pills_per_day;
        pdata.pills_depleted_date = getUtcTime() + (days_to_depletion * 86400UL);
    }

    savePersistentData();
    showConfirmationPopup("PILL TAKEN", "Press button");
    persistent_data_dump("After processing pill taken");
}