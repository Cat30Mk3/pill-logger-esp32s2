// Date and time functions using a DS3231 RTC connected via I2C and Wire lib
#include <Arduino.h>
#include <esp_sleep.h>
#include "globals.h"
#include "core_system.h"
#include "time_logic.h"
#include "data_store.h"
#include "display_ui.h"
#include "battery.h"
#include "RTClib.h"

RTC_DS3231 rtc;

char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

void setup()
{

#if SERIAL_DEBUG_ENABLE
    renderSetupScreen(SETUP_STATE_SERIAL_WAIT);
    DBG_DELAY(2000);
    Serial.begin(115200);
    while (!Serial && millis() < 5000)
    {
        DBG_DELAY(100);
    }

#if LED_ENABLE
    digitalWrite(LED_BUILTIN, HIGH);
#endif
    DBG_DELAY(5000); // Wait for USB CDC to enumerate
    DBG_PRINTLN("");
    DBG_PRINT("Starting DS3231 - ");
    DBG_PRINTLN(VERSION_NAME);
    renderSetupScreen(SETUP_INITIALIZATION);
#endif

    if (!rtc.begin())
    {
        DBG_PRINTLN("Couldn't find RTC");
        Serial.flush();
        while (1)
            delay(10);
    }

    if (rtc.lostPower())
    {
        DBG_PRINTLN("RTC lost power, let's set the time!");
        // When time needs to be set on a new device, or after a power loss, the
        // following line sets the RTC to the date & time this sketch was compiled
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
        // This line sets the RTC with an explicit date & time, for example to set
        // January 21, 2014 at 3am you would call:
        // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
    }

    // When time needs to be re-set on a previously configured device, the
    // following line sets the RTC to the date & time this sketch was compiled
    // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
}

void loop()
{
    DateTime now = rtc.now();
    if (rtc.lostPower())
    {
        DBG_PRINTLN("LOST POWER");
    }
    else
    {
        DBG_PRINTLN("POWER OK");
    }

    DBG_PRINT(now.year());
    DBG_PRINT('/');
    DBG_PRINT(now.month());
    DBG_PRINT('/');
    DBG_PRINT(now.day());
    DBG_PRINT(" (");
    DBG_PRINT(daysOfTheWeek[now.dayOfTheWeek()]);
    DBG_PRINT(") ");
    DBG_PRINT(now.hour());
    DBG_PRINT(':');
    DBG_PRINT(now.minute());
    DBG_PRINT(':');
    DBG_PRINT(now.second());
    DBG_PRINTLN();

    DBG_PRINT(" since midnight 1/1/1970 = ");
    DBG_PRINT(now.unixtime());
    DBG_PRINT("s = ");
    DBG_PRINT(now.unixtime() / 86400L);
    DBG_PRINTLN("d");

    // calculate a date which is 7 days, 12 hours, 30 minutes, 6 seconds into the future
    DateTime future(now + TimeSpan(7, 12, 30, 6));

    DBG_PRINT(" now + 7d + 12h + 30m + 6s: ");
    DBG_PRINT(future.year());
    DBG_PRINT('/');
    DBG_PRINT(future.month());
    DBG_PRINT('/');
    DBG_PRINT(future.day());
    DBG_PRINT(' ');
    DBG_PRINT(future.hour());
    DBG_PRINT(':');
    DBG_PRINT(future.minute());
    DBG_PRINT(':');
    DBG_PRINT(future.second());
    DBG_PRINTLN();

    DBG_PRINT("Temperature: ");
    DBG_PRINT(rtc.getTemperature());
    DBG_PRINTLN(" C");

    DBG_PRINTLN();
    delay(3000);
}