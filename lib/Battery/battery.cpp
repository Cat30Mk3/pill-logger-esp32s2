#include <Arduino.h>
#include "esp_adc_cal.h"
#include "driver/adc.h"
#include "battery.h"
#include "globals.h"

// === Global Battery State ===
static esp_adc_cal_characteristics_t adc_chars;
static float current_voltage_v = 0.0f;
static uint8_t current_soc = 0;
static bool usb_powered = false;
static bool measurement_error = false;
static int calibration_type = 0;  // 1=eFuse two-point, 2=default

// === Initialization ===
void initBattery()
{
    DBG_PRINTLN("[BATTERY] Initializing ADC for battery monitoring...");
    
    // Configure ADC1 channel 8
    adc1_config_width(ADC_WIDTH_BIT_13);
    adc1_config_channel_atten(ADC1_CHANNEL_8, ADC_ATTEN);
    
    // Load factory calibration (two-point from eFuse)
    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
        ADC_UNIT_1,
        ADC_ATTEN,
        ADC_WIDTH_BIT_13,
        0,  // default_vref ignored on ESP32-S2
        &adc_chars);
    
    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        calibration_type = 1;
        DBG_PRINTLN("[BATTERY] Factory two-point (eFuse) calibration loaded");
    } else {
        calibration_type = 2;
        DBG_PRINTLN("[BATTERY] Warning: Using default calibration (less accurate)");
    }
}

// === Core Measurement ===
void readBatteryVoltage()
{
    measurement_error = false;
    uint32_t sum = 0;
    
    // Collect NUM_SAMPLES readings
    for (int i = 0; i < NUM_SAMPLES; i++) {
        int raw = adc1_get_raw(ADC1_CHANNEL_8);
        sum += raw;
        delayMicroseconds(100);
    }
    
    uint32_t avg_raw = sum / NUM_SAMPLES;
    
    // Convert raw to calibrated millivolts
    uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(avg_raw, &adc_chars);
    
    // Convert to volts and apply offset
    float vadc = (voltage_mv / 1000.0f) + ADC_OFFSET_ADJUSTMENT;
    
    // Back-calculate battery voltage from divider
    const float divider_ratio = (Rbottom / (Rtop + Rbottom));
    current_voltage_v = (vadc / divider_ratio) * DIVIDER_ADJUSTMENT;
    
    // Check for error conditions
    if (current_voltage_v < VBAT_MIN_VALID || current_voltage_v > VBAT_MAX_VALID) {
        measurement_error = true;
        current_soc = 0;
        usb_powered = false;
        DBG_PRINTF("[BATTERY] ERROR: Out of range voltage: %.2f V\n", current_voltage_v);
        return;
    }
    
    // Detect USB power
    if (current_voltage_v > USB_THRESHOLD) {
        usb_powered = true;
        current_soc = 100;  // USB is always 100%
        DBG_PRINTF("[BATTERY] USB Power detected: %.2f V\n", current_voltage_v);
        return;
    }
    
    usb_powered = false;
    
    // Map Vbat to SOC using 20% steps
    if (current_voltage_v >= VBAT_100_THRESHOLD) {
        current_soc = 100;
    } else if (current_voltage_v >= VBAT_80_THRESHOLD) {
        current_soc = 80;
    } else if (current_voltage_v >= VBAT_60_THRESHOLD) {
        current_soc = 60;
    } else if (current_voltage_v >= VBAT_40_THRESHOLD) {
        current_soc = 40;
    } else if (current_voltage_v >= VBAT_20_THRESHOLD) {
        current_soc = 20;
    } else if (current_voltage_v >= VBAT_10_THRESHOLD) {
        current_soc = 10;
    } else {  // current_voltage_v >= VBAT_0_THRESHOLD
        current_soc = 0;
    }
    
    DBG_PRINTF("[BATTERY] Vbat: %.2f V, SOC: %u%%\n", current_voltage_v, current_soc);
}

// === State Query Functions ===
uint8_t getBatterySoc()
{
    return current_soc;
}

const char* getBatteryStatus()
{
    if (measurement_error) {
        return "ERROR";
    }
    
    if (usb_powered) {
        return "USB Power";
    }
    
    // Map SOC to status text
    if (current_soc >= 60) {
        return "Full";
    } else if (current_soc >= 20) {
        return "OK";
    } else if (current_soc >= 10) {
        return "Recharge Soon";
    } else {  // 0%
        return "Critical – Recharge Now";
    }
}

float getBatteryVoltage()
{
    if (measurement_error) {
        return VBAT_ERROR_DISPLAY;
    }
    return current_voltage_v;
}

bool isUsbPowered()
{
    return usb_powered;
}

bool isBatteryError()
{
    return measurement_error;
}

// === Debug Output ===
void batteryDumpStatus()
{
    DBG_PRINTLN("\n=== BATTERY STATUS ===");
    DBG_PRINTF("Voltage: %.2f V\n", getBatteryVoltage());
    DBG_PRINTF("SOC: %u%%\n", getBatterySoc());
    DBG_PRINTF("Status: %s\n", getBatteryStatus());
    DBG_PRINTF("USB Powered: %s\n", isUsbPowered() ? "YES" : "NO");
    DBG_PRINTF("Error: %s\n", isBatteryError() ? "YES" : "NO");
    DBG_PRINTF("Calibration Type: %d\n", calibration_type);
    DBG_PRINTLN("======================\n");
}


// Temporary test function - remove after testing
void battery_test_inject_error() {
    measurement_error = true;
    current_voltage_v = 9.99f;
    current_soc = 0;
    usb_powered = false;
    DBG_PRINTLN("[BATTERY TEST] Error injected - 9.99V ERROR should display");
}