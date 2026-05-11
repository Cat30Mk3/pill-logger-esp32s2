#include <Arduino.h>
#include <WiFi.h>
#include <time.h>
#include "esp_wifi.h"
#include <sys/time.h>
#include <Wire.h>
#include <U8g2lib.h>

//================================================
// 2026-02-22 - Pill logger - Wemos ESP32 S2 mini
// Main milestone 2: Display date/time on OLED, and test deep sleep power draw
//
// It connects to WiFi, gets the current time from an NTP server,
// and displays the date and time on an OLED display.
//  It also includes a test of deep sleep power draw.

const char *ssid = "JohnAndLinda";
const char *password = "7054563991";
const char *ntpServer = "pool.ntp.org";
const char *tzInfo = "EST5EDT,M3.2.0/2,M11.1.0/2";

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
#define ADC_PIN 9 // GPIO 9 (ADC1_CH8)
#define Rtop 468.0f //480.f
#define Rbottom 440.9f //335.f
const float VREF = 2.65f;                           // assumed ADC full-scale
const float DIV_RATIO = Rbottom / (Rbottom + Rtop); // ≈ 0.4518

float readBatteryVoltage()
{
    const int samples = 100;
    uint32_t acc = 0;
    for (int i = 0; i < samples; ++i)
    {
        acc += analogRead(ADC_PIN); 
    }
    float rawCount = acc / float(samples);
    float vadc = (rawCount / 4095.0f) * VREF;
    float vbat = vadc / DIV_RATIO;
    return vbat;
}

void setup()
{
    u8g2.begin();
    Serial.begin(115200);
    delay(300);

    analogReadResolution(12); // 12-bit resolution (0-4095)

    Serial.println("\nResetting WiFi...");
    WiFi.disconnect(true, true);
    delay(200);

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    esp_wifi_set_ps(WIFI_PS_NONE);

    Serial.println("Connecting to WiFi...");
    WiFi.begin(ssid, password);

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 10000)
    {
        Serial.print(".");
        delay(250);
    }

    if (WiFi.status() != WL_CONNECTED)
    {
        Serial.println("\nWiFi FAILED to connect.");
        return;
    }

    Serial.println("\nWiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    configTime(0, 0, ntpServer);
    setenv("TZ", tzInfo, 1);
    tzset();

    Serial.println("Waiting for time sync...");
    delay(2000);
}

void loop()
{

    time_t now = time(nullptr);
    struct tm localTime;
    localtime_r(&now, &localTime);
    char dateTimeStr[2][40];
    char DOW[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    char batteryStr[20];

    snprintf(dateTimeStr[0], sizeof(dateTimeStr[0]), "%s %04d-%02d-%02d",
             DOW[localTime.tm_wday],
             localTime.tm_year + 1900,
             localTime.tm_mon + 1,
             localTime.tm_mday);




    snprintf(dateTimeStr[1], sizeof(dateTimeStr[1]), "%02d:%02d:%02d %s",
             localTime.tm_hour,
             localTime.tm_min,
             localTime.tm_sec,
             localTime.tm_isdst ? "EDT" : "EST");

    Serial.printf("Local time: %s %s\n", dateTimeStr[0], dateTimeStr[1]);

    float vbat = readBatteryVoltage();
    snprintf(batteryStr, sizeof(batteryStr), "Battery: %.2f V", vbat);
    Serial.println(batteryStr);

    u8g2.clearBuffer();
    drawCentered(dateTimeStr[1], 10, u8g2_font_lucasfont_alternate_tf);
    drawCentered(dateTimeStr[0], 20, u8g2_font_lucasfont_alternate_tf);
    drawCentered(batteryStr, 30, u8g2_font_lucasfont_alternate_tf);
    // u8g2.drawFrame(0, 0, 128, 32);
    u8g2.sendBuffer();

    delay(1000);
} 

