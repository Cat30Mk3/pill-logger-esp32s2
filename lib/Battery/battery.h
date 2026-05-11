#pragma once
#include <stdint.h>
#include <stdbool.h>

// Initialization (call from main setup after initCoreSystem)
void initBattery();

// Core measurement and conversion functions
void readBatteryVoltage();           // Measure voltage, update all battery state variables
uint8_t getBatterySoc();             // Return 0, 10, 20, 30, 40, 50, 60, 70, 80, 90, or 100
const char* getBatteryStatus();      // Return "Full", "OK", "Recharge Soon", "Critical", "USB Power", or "ERROR"
float getBatteryVoltage();           // Return measured Vbat in volts
bool isUsbPowered();                 // Return true if USB power detected
bool isBatteryError();               // Return true if measurement/conversion error occurred

// Debug function
void batteryDumpStatus();            // Print all battery state to serial
