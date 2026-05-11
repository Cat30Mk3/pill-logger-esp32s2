// --- Inactivity timer global ---
extern uint32_t lastActivityTime;
// --- Button edge flags (for global access) ---
extern volatile bool pb_falling_detected;
extern volatile bool pb_rising_detected;
#pragma once
#include <stdint.h>
#include <stdbool.h>


// --- RTC Fast Memory State ---
struct RtcState {
    uint32_t magic;                 // Magic number for integrity
    uint8_t  screen_index;          // Current screen index
    uint32_t last_wake_timestamp;   // For inactivity timer
    uint8_t  flags;                 // Future use
};

extern RtcState rtcState;


// Magic number constant
static const uint32_t RTC_MAGIC = 0x504C4F47; // 'PLOG'

// --- Button state flags (for debug monitoring) ---
extern volatile bool pb_short_press_detected;
extern volatile bool pb_long_press_detected;


// --- ISR Test Function ---    
// bool testAndClearButtonFlag();

// --- Core System Initialization ---
void initCoreSystem();
void loadRtcState();
void saveRtcState();

// --- Wake Handling ---

// --- Deep Sleep ---
void enterDeepSleep();

// --- PB_TOP Timer-Based Button Handler (NEW) ---
void pb_top_init();      // Initialize timer and ISR (called in initCoreSystem)
void pb_top_enable();    // Enable button detection (called after setup complete)
void pb_top_disable();   // Disable button detection (for setup phase)
bool pb_top_is_short_press();  // Check if short press detected
bool pb_top_is_long_press();   // Check if long press detected

// --- Pill Processing ---
void processPillTakenLongPress();

// --- Nav Button ISR Handlers (PB_LEFT, PB_RIGHT, PB_SELECT) ---
void pbLeftIsrHandler();
void pbRightIsrHandler();
void pbSelectIsrHandler();

// --- Nav Button Processing (PB_LEFT, PB_RIGHT, PB_SELECT) ---
// Call from main loop. Updates inactivity timer on each press.
void processNavButtonEvents();
bool isLeftPress();
bool isRightPress();
bool isSelectPress();
void clearAllNavPressFlags();

// --- Time Helpers ---
uint64_t getIsrTimestamp();