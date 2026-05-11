#include <Arduino.h>

const float VREF      = 2.45f;      // assumed ADC full-scale
const float DIV_RATIO = 387.2f / (469.8f + 387.2f);  // ≈ 0.4518

float readBatteryVoltage() {
    const int samples = 32;
    uint32_t acc = 0;
    for (int i = 0; i < samples; ++i) {
        acc += analogRead(9);   // GPIO 9 (ADC1_CH8)
    }
    float raw = acc / float(samples);
    float vadc = (raw / 4095.0f) * VREF;
    float vbat = vadc / DIV_RATIO;
    return vbat;
}