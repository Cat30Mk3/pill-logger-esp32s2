#include <Arduino.h>
#include <time.h>
#include <sys/time.h>
#include <Wire.h>
#include <U8g2lib.h>


#define SCL_PIN 35
#define SDA_PIN 33



// Pin definitions (shared)
#define PB_TOP_PIN    1
#define PB_LEFT_PIN   4
#define PB_RIGHT_PIN  7
#define PB_SELECT_PIN 5
// Hardware-confirmed pin assignments: PB_LEFT=4, PB_RIGHT=7, PB_SELECT=5
#define LED_BUILTIN   15



// U8G2 constructor for SSD1306 128x32 I2C (non‑paged, full buffer)
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(
    U8G2_R0, // rotation
    /* reset=*/U8X8_PIN_NONE,
    /* clock=*/SCL_PIN,
    /* data=*/SDA_PIN);

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

    pinMode(PB_TOP_PIN, INPUT_PULLUP);
    pinMode(PB_RIGHT_PIN, INPUT_PULLUP);
    pinMode(PB_LEFT_PIN, INPUT_PULLUP);
    pinMode(PB_SELECT_PIN, INPUT_PULLUP);
    Serial.begin(115200);
    u8g2.begin();
    delay(5000);

    Serial.println("\nStarting Push Button Test…");
}

void loop()
{

    if (digitalRead(PB_TOP_PIN) == LOW)
    {
        Serial.println("Top Button pressed! ");

        u8g2.clearBuffer();
        drawCentered("Top Button pressed!", 14, u8g2_font_lucasfont_alternate_tf);
        u8g2.sendBuffer();
    }
    else if (digitalRead(PB_RIGHT_PIN) == LOW)
    {
        Serial.println("Right Button pressed! ");
        u8g2.clearBuffer();
        drawCentered("Right Button pressed!", 14, u8g2_font_lucasfont_alternate_tf);
        u8g2.sendBuffer();
    }
    else if (digitalRead(PB_LEFT_PIN) == LOW)
    {
        Serial.println("Left Button pressed! ");
        u8g2.clearBuffer();
        drawCentered("Left Button pressed!", 14, u8g2_font_lucasfont_alternate_tf);
        u8g2.sendBuffer();
    }
        else if (digitalRead(PB_SELECT_PIN) == LOW)
    {
        Serial.println("Select Button pressed! ");
        u8g2.clearBuffer();
        drawCentered("Select Button pressed!", 14, u8g2_font_lucasfont_alternate_tf);
        u8g2.sendBuffer();
    }
     else
    {
        //Serial.println("All Buttons released. ");
        u8g2.clearBuffer();
        drawCentered("All Buttons Released.", 14, u8g2_font_lucasfont_alternate_tf);
        u8g2.sendBuffer();
    }
    delay(100);
}
