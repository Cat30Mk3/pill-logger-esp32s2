#include <Arduino.h>
#include "esp_adc_cal.h"
#include "driver/adc.h"
#include <Wire.h>
#include <U8g2lib.h>

// Global (or static) characteristics struct
esp_adc_cal_characteristics_t adc_chars;
int calType;

// U8G2 constructor for SSD1306 128x32 I2C (non‑paged, full buffer)
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(
    U8G2_R0, // rotation
    /* reset=*/U8X8_PIN_NONE,
    /* clock=*/35,
    /* data=*/33);

void drawCentered(const char *str, int y, const uint8_t *font = u8g2_font_ncenB08_tr)
{
  u8g2.setFont(font);
  int textWidth = u8g2.getStrWidth(str);
  int oledWidth = u8g2.getDisplayWidth();
  int x = (oledWidth - textWidth) / 2;
  u8g2.drawStr(x, y, str);
}

void setup()
{
  u8g2.begin();

  Serial.begin(115200);
  delay(5000);
  Serial.println("Starting ADC voltage measurement...");

  // Configure ADC (do this once)
  adc1_config_width(ADC_WIDTH_BIT_13);                        // ESP32-S2 uses 13-bit on early revisions (v0.0); later revisions are 12-bit
  adc1_config_channel_atten(ADC1_CHANNEL_8, ADC_ATTEN_DB_11); // 11 dB attenuation → safe up to ~2500 mV (your divider output max ≈ 2.16 V)

  // Characterize ADC → this reads the factory eFuse two-point data and builds the calibration curve
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
      ADC_UNIT_1, // ADC1
      ADC_ATTEN_DB_11,
      ADC_WIDTH_BIT_13,
      0, // default_vref is ignored on S2 (two-point only)
      &adc_chars);

  // Optional: print which calibration was used
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP)
  {
    calType = 1;
    Serial.println("Factory two-point (eFuse) calibration curve loaded successfully");
  }
  else
  {
    calType = 2;
    Serial.println("Warning: Falling back to default calibration (less accurate)");
  }
}

// === After the setup() code above ===
float Vadc; // Global variable to hold the last ADC voltage reading

// calibration adjustments - refer to the Excell spread sheet "ADC Calibration.xlsx" for details on how these were derived
float adcOffsetAdjustment = -0.00600f; //  consistent offset adjustment
// float dividerAdjustment = 1.2614f;       // voltage divider adjustment (unloaded)
float dividerAdjustment = 1.067273f; // voltage divider adjustment
float readBatteryVoltage()
{
  const int NUM_SAMPLES = 32; // 32 samples gives excellent noise reduction
  uint32_t sum = 0;

  for (int i = 0; i < NUM_SAMPLES; i++)
  {
    int raw = adc1_get_raw(ADC1_CHANNEL_8);
    sum += raw;
    delayMicroseconds(100); // small delay between samples helps reduce noise
  }

  uint32_t avg_raw = sum / NUM_SAMPLES;

  // Convert raw ADC reading to calibrated millivolts using manufacturer's curve
  uint32_t voltage_mv = esp_adc_cal_raw_to_voltage(avg_raw, &adc_chars);

  //  exact voltage divider (Rtop = 470.0 kΩ from battery+, Rbottom = 496.5 kΩ to GND - both 470k nominal)  
  const float Rtop = 470000.0f;                             // ohms
  const float Rbottom = 496500.0f;                          // ohms
  const float divider_ratio = (Rbottom / (Rtop + Rbottom)); // ≈ 0.5137

  Vadc = (voltage_mv / 1000.0f) + adcOffsetAdjustment;     //  convert mV to V for clarity and apply adc offset adjustment
  float Vbus = (Vadc / divider_ratio) * dividerAdjustment; // Adjust for the voltage divider and any calibration adjustments

  return Vbus;
}

void loop()
{
  char buffer[2][32];
  float Vbus = readBatteryVoltage();
  
  sprintf(buffer[0], "ADC: %.3f V", Vadc);
  sprintf(buffer[1], "Vbus: %.3f V", Vbus);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_ncenB08_tr);
  drawCentered(buffer[0], 14, u8g2_font_lucasfont_alternate_tf);
  drawCentered(buffer[1], 28, u8g2_font_lucasfont_alternate_tf);
  u8g2.drawFrame(0, 0, 128, 32);
  u8g2.sendBuffer();
  Serial.printf("calType: %d, ADC voltage: %.3f V, Battery voltage: %.3f V\n", calType, Vadc, Vbus);
  delay(1000);
}