#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// U8G2 constructor for SSD1306 128x32 I2C (non‑paged, full buffer)
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(
    U8G2_R0, // rotation
    /* reset=*/U8X8_PIN_NONE,
    /* clock=*/35,
    /* data=*/33);

// U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0);

void printWakeCause()
{
    esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();

    switch (cause)
    {
    case ESP_SLEEP_WAKEUP_TIMER:
        Serial.println("Wake cause: Timer");
        break;

    case ESP_SLEEP_WAKEUP_EXT0:
        Serial.println("Wake cause: EXT0 (single pin)");
        break;

    case ESP_SLEEP_WAKEUP_EXT1:
        Serial.println("Wake cause: EXT1 (multiple pins)");
        break;

    case ESP_SLEEP_WAKEUP_UNDEFINED:
    default:
        Serial.println("Wake cause: Cold boot");
        break;
    }
}

int centreString(const char *str)
{
    int textWidth = u8g2.getStrWidth(str);
    int oledWidth = u8g2.getDisplayWidth();
    int startXPosn = (oledWidth - textWidth) / 2;
    return startXPosn;
}

void setup()
{
    // Mandatory stabilization delay after deep sleep
    // delay(200);

    Serial.begin(115200);
    delay(2000);

    Serial.println("=== ESP32 Deep Sleep Test ===");
    printWakeCause();

    // // Reset I2C after deep sleep
    // Wire.end();
    // delay(10);
    // Wire.begin();
    // delay(10);

    // Initialize display safely
    u8g2.begin();
    delay(100);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_lucasfont_alternate_tf);
    u8g2.drawStr(centreString("Deep sleep test..."), 16, "Deep sleep test...");
    u8g2.drawFrame(0, 0, 128, 32);
    u8g2.sendBuffer();
    delay(5000);

    //now clear display to see what power draw is left to compare with setPowerSave(1) mode
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    delay(1000);

    // Set display to power save mode (turn off OLED)
    u8g2.setPowerSave(1);
    delay(1000);

    //now enter deep sleep for 10 seconds (10,000,000 microseconds)
    esp_sleep_enable_timer_wakeup(10ULL * 1000000ULL);
    esp_deep_sleep_start();
}

void loop()
{
    // This will never be called due to deep sleep

    delay(1000);
    Serial.println("This should never print!");
}