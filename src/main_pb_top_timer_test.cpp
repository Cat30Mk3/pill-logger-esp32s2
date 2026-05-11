#include <Arduino.h>
#include <esp_timer.h>
#include "globals.h"

// ============================================================================
// PB_TOP TIMER-BASED BUTTON HANDLER TEST
// ============================================================================
// Test program for new timer-based PB_TOP button detection
// Existing nav buttons (PB_LEFT, PB_RIGHT, PB_SELECT) unchanged
// Built with intention to migrate to production core_system library

// ============================================================================
// PB_TOP TIMER-BASED STATE MACHINE
// ============================================================================

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
    .long_press_ready = false,
    .timer_handle = NULL,
    .timer_active = false,
    .pb_top_enabled = true
};

// Constants (milliseconds) - using globals.h definitions where available
// Additional PB_TOP-specific constants not in globals.h:
const uint32_t WATCHDOG_TIMEOUT_MS = 8000;  // 2x LONG_PRESS_THRESHOLD_MS
const uint32_t STABLE_RELEASE_MS = 20;      // Debounce on release

// ============================================================================
// TIMER CALLBACK (runs independently of main loop)
// ============================================================================

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
            Serial.printf("[PB_TOP] Debounce passed, now MEASURING (press at %lu ms)\n", pb_top.press_fall_time_ms);
        }
        break;
        
    case PbTopTimerState::MEASURING:
        // Watching hold duration, waiting for RISING edge (will be handled in ISR)
        // But watchdog if stuck too long
        if (elapsed_ms > WATCHDOG_TIMEOUT_MS) {
            Serial.printf("[PB_TOP WATCHDOG] Stuck in MEASURING for %lu ms, resetting\n", elapsed_ms);
            pb_top.phase = PbTopTimerState::IDLE;
            pb_top.phase_start_us = 0;
        }
        break;
        
    case PbTopTimerState::LATCHING:
        // Waiting for stable HIGH release before re-arming
        if (digitalRead(PB_TOP_PIN) == HIGH) {
            if (elapsed_ms >= STABLE_RELEASE_MS) {
                pb_top.phase = PbTopTimerState::IDLE;
                pb_top.phase_start_us = 0;
                Serial.printf("[PB_TOP] Re-armed (stable release detected)\n");
            }
        } else {
            // Button pressed again during latching - restart
            pb_top.phase_start_us = now_us;
        }
        break;
        
    default:
        break;
    }
}

// ============================================================================
// PB_TOP ISR (called on every edge: FALLING or RISING)
// ============================================================================

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
            Serial.printf("[PB_TOP ISR] FALLING edge, entering DEBOUNCE\n");
        }
    } else {
        // RISING edge (button released)
        if (pb_top.phase == PbTopTimerState::MEASURING) {
            pb_top.press_rise_time_ms = now_ms;
            pb_top.hold_duration_ms = pb_top.press_rise_time_ms - pb_top.press_fall_time_ms;
            
            // Sanity check hold duration
            if (pb_top.hold_duration_ms < DEBOUNCE_TIME_MS || pb_top.hold_duration_ms > 10000) {
                Serial.printf("[PB_TOP ISR] Invalid hold duration %lu ms, ignoring\n", pb_top.hold_duration_ms);
                pb_top.phase = PbTopTimerState::LATCHING;
            } else {
                // Valid hold duration - determine press type
                if (pb_top.hold_duration_ms >= LONG_PRESS_THRESHOLD_MS) {
                    pb_top.long_press_ready = true;
                    pb_top.last_long_press_ms = pb_top.hold_duration_ms;
                    Serial.printf("[PB_TOP ISR] LONG PRESS detected (%lu ms)\n", pb_top.hold_duration_ms);
                } else if (pb_top.hold_duration_ms >= SHORT_PRESS_MIN_MS) {
                    pb_top.short_press_ready = true;
                    pb_top.last_short_press_ms = pb_top.hold_duration_ms;
                    Serial.printf("[PB_TOP ISR] SHORT PRESS detected (%lu ms)\n", pb_top.hold_duration_ms);
                } else {
                    Serial.printf("[PB_TOP ISR] Hold too short (%lu ms), noise, ignoring\n", pb_top.hold_duration_ms);
                }
                pb_top.phase = PbTopTimerState::LATCHING;
            }
            pb_top.phase_start_us = now_us;
        }
    }
}

// ============================================================================
// PUBLIC API
// ============================================================================

void pb_top_init()
{
    pinMode(PB_TOP_PIN, INPUT_PULLUP);
    
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
        Serial.println("[PB_TOP] Timer initialized and started");
    } else {
        Serial.println("[PB_TOP ERROR] Failed to create timer");
    }
    
    // Attach ISR to handle edge detection
    attachInterrupt(PB_TOP_PIN, pb_top_isr_handler, CHANGE);
    Serial.println("[PB_TOP] ISR attached to CHANGE");
    
    // Initially disabled (will be enabled after setup complete)
    pb_top.pb_top_enabled = false;
    Serial.println("[PB_TOP] Initially DISABLED");
}

void pb_top_enable()
{
    pb_top.pb_top_enabled = true;
    Serial.println("[PB_TOP] ENABLED");
}

void pb_top_disable()
{
    pb_top.pb_top_enabled = false;
    pb_top.short_press_ready = false;
    pb_top.long_press_ready = false;
    pb_top.phase = PbTopTimerState::IDLE;
    Serial.println("[PB_TOP] DISABLED");
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

// ============================================================================
// EXISTING NAV BUTTON HANDLERS (unchanged from core_system.cpp)
// ============================================================================

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

void IRAM_ATTR pbLeftIsrHandler() {
    onNavFalling(navLeft);
}

void IRAM_ATTR pbRightIsrHandler() {
    onNavFalling(navRight);
}

void IRAM_ATTR pbSelectIsrHandler() {
    onNavFalling(navSelect);
}

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

void nav_buttons_init()
{
    pinMode(PB_LEFT_PIN,   INPUT_PULLUP);
    attachInterrupt(PB_LEFT_PIN,   pbLeftIsrHandler,   FALLING);
    pinMode(PB_RIGHT_PIN,  INPUT_PULLUP);
    attachInterrupt(PB_RIGHT_PIN,  pbRightIsrHandler,  FALLING);
    pinMode(PB_SELECT_PIN, INPUT_PULLUP);
    attachInterrupt(PB_SELECT_PIN, pbSelectIsrHandler, FALLING);
    Serial.println("[NAV BUTTONS] Initialized");
}

// ============================================================================
// TEST PROGRAM
// ============================================================================

void setup()
{
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n\n=== PB_TOP TIMER-BASED BUTTON TEST ===\n");
    
    // Initialize all buttons
    pb_top_init();
    nav_buttons_init();
    
    Serial.println("\nSetup phase: PB_TOP DISABLED for 5 seconds...");
    Serial.println("(Simulating Serial/WiFi initialization delays)\n");
}

void loop()
{
    // Simulate setup delay for first 5 seconds
    static bool setup_complete = false;
    static uint32_t setup_start = millis();
    
    if (!setup_complete && (millis() - setup_start) >= 5000) {
        setup_complete = true;
        pb_top_enable();
        Serial.println("\n>>> SETUP COMPLETE - PB_TOP NOW ENABLED <<<\n");
    }
    
    // Process navigation buttons (unchanged)
    processNavButtonEvents();
    
    // Check for button events
    if (pb_top_is_short_press()) {
        Serial.println("[MAIN] PB_TOP SHORT PRESS - cycle screen");
    }
    
    if (pb_top_is_long_press()) {
        Serial.printf("[MAIN] PB_TOP LONG PRESS - pill taken action (%lu ms)\n", pb_top.last_long_press_ms);
    }
    
    if (isLeftPress()) {
        Serial.println("[MAIN] PB_LEFT pressed");
    }
    
    if (isRightPress()) {
        Serial.println("[MAIN] PB_RIGHT pressed");
    }
    
    if (isSelectPress()) {
        Serial.println("[MAIN] PB_SELECT pressed");
    }
    
    // Serial commands for testing
    if (Serial.available()) {
        char cmd = Serial.read();
        switch (cmd) {
            case 'd':
                pb_top_disable();
                break;
            case 'e':
                pb_top_enable();
                break;
            case 's':
                Serial.printf("[STATUS] PB_TOP phase: %u, enabled: %s\n", 
                    (unsigned)pb_top.phase, pb_top.pb_top_enabled ? "YES" : "NO");
                break;
            case '?':
                Serial.println("\nCommands:");
                Serial.println("  d - Disable PB_TOP");
                Serial.println("  e - Enable PB_TOP");
                Serial.println("  s - Status");
                Serial.println("  ? - This help\n");
                break;
        }
    }
    
    delay(10);
}
