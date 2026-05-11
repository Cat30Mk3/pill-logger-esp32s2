#include "globals.h"
#include <string.h>

#define I2C_SDA 33
#define I2C_SCL 35

U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(
	U8G2_R2, // rotation 180 degrees (default)
	/* reset=*/U8X8_PIN_NONE,
	/* clock=*/I2C_SCL,
	/* data=*/I2C_SDA);

uint32_t lastActivityTime = 0;

char ssid_buffer[33]; 

float battery_voltage_v = 3.95f;
uint8_t battery_capacity_pct = 75;
char battery_condition[16] = "Placeholder";

// Edit mode active flag (Stage 1 - Milestone 4)
bool g_edit_mode_active = false;

// Edit session state (Stage 2 - Milestone 4)
EditState g_edit_state = EDIT_NONE;

// Setup status globals
volatile uint8_t g_current_setup_state = SETUP_STATE_SERIAL_WAIT;
bool g_setup_complete = false;

