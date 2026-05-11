#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "esp_wifi.h"
#include <sys/time.h>
#include <Wire.h>
#include <U8g2lib.h>



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
#define Rtop 468.0f //470k nominal measured with multimeter as 468k 
#define Rbottom 440.9f //470k nominal measured with multimeter as 440.9k
const float VREF = 2.65f;                           // assumed ADC full-scale
const float DIV_RATIO = Rbottom / (Rbottom + Rtop); // ≈ 0.4518



void setup()
{
    u8g2.begin();
    Serial.begin(115200);
    delay(300);

    analogReadResolution(12); // 12-bit resolution (0-4095)

   
}


void loop()
{
    // float vbat = readBatteryVoltage();
    char batteryStr[2][30];

    const int samples = 32;
    uint32_t acc = 0;
    for (int i = 0; i < samples; ++i)
    {
        acc += analogRead(9);
    }
    float rawCount = acc / float(samples);
    float vadc = (rawCount / 4095.0f) * VREF;
    float vAlogRead = float(analogReadMilliVolts(9)) / float(1000); // Read voltage in millivolts directly from ADC
    float vbat = vadc / DIV_RATIO;
    float vbatAlogRead = vAlogRead / DIV_RATIO;

    Serial.print("rawCount = ");
    Serial.print(rawCount);
    Serial.print("  vadc = ");
    Serial.print(vadc, 3);
    Serial.print(" V  vAlogRead = ");
    Serial.print(vAlogRead, 3);
    Serial.print(" V  vbat = ");
    Serial.print(vbat, 3);
    Serial.print(" V  vbatAlogRead = ");
    Serial.print(vbatAlogRead, 3);
    Serial.println(" V");


    strcpy(batteryStr[0], ""); // Clear the string
    strcpy(batteryStr[1], ""); // Clear the string

    snprintf(batteryStr[0], sizeof(batteryStr[0]), "cnt:%0.1f Vadc:%0.3f ", rawCount, vadc);
    snprintf(batteryStr[1], sizeof(batteryStr[1]), "Vbat:%0.3f ", vbat);
    u8g2.clearBuffer();
    drawCentered(batteryStr[0], 16, u8g2_font_lucasfont_alternate_tf);
    drawCentered(batteryStr[1], 28, u8g2_font_lucasfont_alternate_tf);
    u8g2.drawFrame(0, 0, 128, 32);
    u8g2.sendBuffer();

    delay(1000);
}
