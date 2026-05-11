#include <Arduino.h>
#include <time.h>
#include <sys/time.h>
#include <Wire.h>
#include <U8g2lib.h>

#define OLED_POWER_PIN 6 // gate of n-chn mosfet in series with OLED GND pin, gate driven by GPIO6. This allows us to power cycle the OLED by toggling GPIO6, which can be useful for resetting the OLED if it gets into a bad state. Note that the OLED will be off (powered down) when GPIO6 is LOW, and on (powered up) when GPIO6 is HIGH.
#define TEST_PB_PIN 3    // connected to a push button that pulls the pin LOW when pressed. This is just for testing that the button works, and is not used in the final pill logger project.
#define TEST_LED_PIN 7   // connected to an LED that we can turn on when the button is pressed, just for testing that the button works. This is not used in the final pill logger project.

/// GLOBAL Variables
boolean OLED_Powered_Down = true;

// U8G2 constructor for SSD1306 128x32 I2C (non‑paged, full buffer)
U8G2_SSD1306_128X32_UNIVISION_F_HW_I2C u8g2(
    U8G2_R0, // rotation
    /* reset=*/U8X8_PIN_NONE,
    /* clock=*/5,
    /* data=*/4);

void setTimeFromCompile()
{
    // 1. Set timezone BEFORE using mktime()
    // Adjust this TZ string to whatever zone you want the ESP32 to use.
    // setenv("TZ", "EST5EDT", 1);
    setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0/2", 1);
    tzset();

    // 2. Parse compile date/time (LOCAL TIME) into a tm struct
    struct tm tm = {0};
    strptime(__DATE__ " " __TIME__, "%b %d %Y %H:%M:%S", &tm);

    // 3. Convert local time → UTC timestamp into a posix time_t (seconds since 1970-01-01 00:00:00 UTC)
    time_t t = mktime(&tm);

    // 4. Apply to system clock (expects UTC) - use settimeofday() which takes a timeval struct
    struct timeval tv = {
        .tv_sec = t,
        .tv_usec = 0};
    settimeofday(&tv, NULL);
    // Note: settimeofday() expects UTC, so we give it the timestamp we got from mktime(), which is in UTC
    // Note that the second argument is always set to NULL -- it is only used if you have an external timezone database,
    // which we do not need here since we hardcoded the TZ string above.

    // 5. Optional: print verification
    // converts the compile time into both UTC and local tm structs so we can print them out in human readable form and verify that the timezone and DST were applied correctly. This is just for verification, the system clock is already set by settimeofday() above.
    struct tm utc_tm;
    struct tm local_tm;

    gmtime_r(&t, &utc_tm);      // cxonverts posix time_t (UTC) back into a tm struct in UTC
    localtime_r(&t, &local_tm); // converts posix time_t (UTC) into a tm struct in local time, applying the timezone and DST rules we set with setenv("TZ", ...)

    Serial.println("=== Compile-Time Clock Set ===");

    Serial.printf("Compile Local Time: %s %s\n", __DATE__, __TIME__);

    Serial.printf("UTC Set:   %04d-%02d-%02d %02d:%02d:%02d\n",
                  utc_tm.tm_year + 1900, utc_tm.tm_mon + 1, utc_tm.tm_mday,
                  utc_tm.tm_hour, utc_tm.tm_min, utc_tm.tm_sec);

    Serial.printf("Local Now: %04d-%02d-%02d %02d:%02d:%02d\n",
                  local_tm.tm_year + 1900, local_tm.tm_mon + 1, local_tm.tm_mday,
                  local_tm.tm_hour, local_tm.tm_min, local_tm.tm_sec);

    Serial.println("==============================");
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

    u8g2.begin();
    pinMode(OLED_POWER_PIN, OUTPUT);
    pinMode(TEST_PB_PIN, INPUT_PULLUP);
    pinMode(TEST_LED_PIN, OUTPUT);
    digitalWrite(OLED_POWER_PIN, LOW); // Turn off OLED power
    OLED_Powered_Down = true;

    Serial.begin(115200);
    delay(500);

    Serial.println("\nStarting compile-time clock…");

    setTimeFromCompile();

    Serial.println("Clock initialized from compile time.");
}

void loop()
{

    // char DOW[7][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    char dateTimeStr[2][20];
    // sprintf(dateTimeStr[0], "%02d:%02d:%02d", hour, minute, second);
    // sprintf(dateTimeStr[1], "%s %02d/%02d/%04d", DOW[6], day, month, year);

    time_t now = time(nullptr);
    struct tm *local = localtime(&now);

    strftime(dateTimeStr[0], sizeof(dateTimeStr[0]), "%H:%M:%S %Z", local);
    strftime(dateTimeStr[1], sizeof(dateTimeStr[1]), "%a %d/%m/%Y", local);
    Serial.println(dateTimeStr[0]);

    u8g2.clearBuffer();
    // u8g2.setFont(u8g2_font_ncenB08_tr);
    u8g2.setFont(u8g2_font_lucasfont_alternate_tf);

    u8g2.drawStr(centreString(dateTimeStr[0]), 14, dateTimeStr[0]);
    u8g2.drawStr(centreString(dateTimeStr[1]), 26, dateTimeStr[1]);
    u8g2.drawFrame(0, 0, 128, 32);
    u8g2.sendBuffer();

    if (digitalRead(TEST_PB_PIN) == LOW)
    {
        digitalWrite(TEST_LED_PIN, HIGH); // Turn on test LED to indicate button press
        Serial.println("Button pressed! Powering up OLED...");
        digitalWrite(OLED_POWER_PIN, HIGH); // Turn on OLED
        delay(500);

        if (OLED_Powered_Down)
        {
            Serial.println("OLED found powered down.");

            digitalWrite(OLED_POWER_PIN, HIGH); // turn MOSFET on
            delay(20);                          // allow OLED to boot

            u8g2.setPowerSave(0); // wake U8g2 internal state
            u8g2.begin();         // FULL re-init
            u8g2.clearBuffer();
            u8g2.sendBuffer();
            OLED_Powered_Down = false;
        }
        else
        {
            Serial.println("OLED already powered up.");
            u8g2.sendBuffer();
        }
    }
    else
    {
        digitalWrite(OLED_POWER_PIN, LOW); // Turn off OLED
        digitalWrite(TEST_LED_PIN, LOW);
        u8g2.setPowerSave(1);
        boolean OLED_Powered_Down = true;
    }

    delay(1000);
}