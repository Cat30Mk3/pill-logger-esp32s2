#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>

// U8G2 constructor for SSD1306 128x32 I2C (non‑paged, full buffer)
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(
    U8G2_R0, // rotation
    /* reset=*/U8X8_PIN_NONE,
    /* clock=*/35,
    /* data=*/33);

// // Software I2C constructor (always works)
// U8G2_SSD1306_128X64_NONAME_F_SW_I2C u8g2(
//     U8G2_R0,
//     /* clock=*/ 5,
//     /* data=*/ 4,
//     /* reset=*/ U8X8_PIN_NONE
// );

void setup()
{
    u8g2.begin();
}

void drawCentered(const char* str, int y, const uint8_t* font = u8g2_font_ncenB08_tr) {
    u8g2.setFont(font);
    int textWidth = u8g2.getStrWidth(str);
    int oledWidth = u8g2.getDisplayWidth();
    int x = (oledWidth - textWidth) / 2;
    u8g2.drawStr(x, y, str);
}



void loop()
{
    int hour = 10;
    int minute = 30;
    int second = 45;
    int day = 20;
    int month = 2;
    int year = 2026;
    char DOW[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    char dateTimeStr[2][10];
    sprintf(dateTimeStr[0], "%02d:%02d:%02d", hour, minute, second);
    sprintf(dateTimeStr[1], "%s %02d/%02d/%04d", DOW[6], day, month, year);
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_ncenB08_tr);
    drawCentered(dateTimeStr[0], 14,u8g2_font_lucasfont_alternate_tf);
    drawCentered(dateTimeStr[1], 26,u8g2_font_lucasfont_alternate_tf);
    u8g2.drawFrame(0, 0, 128, 32);
    u8g2.sendBuffer();
    delay(1000);
}