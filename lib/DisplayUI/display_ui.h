#pragma once
#include <stdint.h>
#include "globals.h"

// --- Display Initialization ---
void initDisplay();

// --- Setup Screen (shown during initialization) ---
void renderSetupScreen(uint8_t setupState);

// --- Screen Rendering (stub screens for Milestone 0) ---
void renderScreen(uint8_t index);

// --- Screen Navigation ---
void nextScreen();
uint8_t getScreenIndex();
void setScreenIndex(uint8_t index);

// --- Confirmation Popup ---
void showConfirmationPopup(const char *line1, const char *line2);
void clearConfirmationPopup();
bool isConfirmationPopupActive();

// --- MUI Edit Mode (Milestone 4) ---
// Entry detection: call each loop when NOT in edit mode.
// Detects PB_RIGHT on primary screen 2 (Rx Edit) or screen 3 (Pills Edit)
// and calls enterEditMode() automatically.
void checkMuiEntry();

// State management
void enterEditMode(EditState state);
void exitEditMode(bool resetActivityTimer = true);

// Called from main loop when g_edit_mode_active is true
void processMuiInput();   // dispatch nav button presses to MUI
void renderEditScreen();  // render the active MUI form