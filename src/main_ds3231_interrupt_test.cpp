
/* DS3231 1 Hz SQW interrupt test
 *
 * VCC and GND of RTC should be connected to some power source
 * SDA, SCL of RTC should be connected to SDA, SCL of arduino
 * SQW should be connected to CLOCK_INTERRUPT_PIN
 * CLOCK_INTERRUPT_PIN needs to work with interrupts
 */

#include <Arduino.h>
#include <esp_sleep.h>
#include "globals.h"
#include "core_system.h"
#include "time_logic.h"
#include "data_store.h"
#include "display_ui.h"
#include "battery.h"
#include <RTClib.h>
// #include <Wire.h>


#define ALARM_MODE 1 // 1=alarm mode, 0=square wave mode

RTC_DS3231 rtc;

// the pin that is connected to SQW
#define CLOCK_INTERRUPT_PIN 12
#define DS3231_VCC_PIN 11

volatile uint32_t sqwFallingEdges = 0;
volatile bool ds3231InterruptFlag = false;


void IRAM_ATTR onSqwFallingEdge() {
    sqwFallingEdges++;
    ds3231InterruptFlag = true;
}

void setup()
{

    pinMode(DS3231_VCC_PIN, OUTPUT);
    digitalWrite(DS3231_VCC_PIN, HIGH);

    // wait for RTC to power up
    delay(1000);

    if (!rtc.begin())
    {
        Serial.println("Couldn't find RTC");
        while (1)
            ;
    }

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

    if (rtc.lostPower())
    {
        // this will adjust to the date and time at compilation
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    // we don't need the 32K Pin, so disable it
    rtc.disable32K();

    // SQW/INT is open-drain, so keep pull-up enabled on the MCU input.
    pinMode(CLOCK_INTERRUPT_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(CLOCK_INTERRUPT_PIN), onSqwFallingEdge, FALLING);


#if ALARM_MODE
    // Disable continuous 1 Hz square-wave output on SQW/INT pin ( alarm mode).
    Serial.println("ALARM MODE");
    rtc.writeSqwPinMode(DS3231_OFF);
    rtc.disableAlarm(2);
    rtc.clearAlarm(1);
    rtc.clearAlarm(2);
    // schedule an alarm 10 seconds in the future
    if (!rtc.setAlarm1(
            rtc.now() + TimeSpan(10),
            DS3231_A1_Second // this mode triggers the alarm when the seconds match. See Doxygen for other options
            ))
    {
        Serial.println("Error, alarm wasn't set!");
    }
    else
    {
        Serial.println("Alarm will happen in 10 seconds!");
    }

#else
    // Enable continuous 1 Hz square-wave output on SQW/INT pin (not alarm mode).
    Serial.println("SQUAREWAVE MODE");
    rtc.writeSqwPinMode(DS3231_SquareWave1Hz);
    // Disable both alarms so SQW pin is used only for square-wave output.
    rtc.disableAlarm(1);
    rtc.disableAlarm(2);
    rtc.clearAlarm(1);
    rtc.clearAlarm(2);
#endif


}

void loop()
{

if (ds3231InterruptFlag)
    {
        ds3231InterruptFlag = false;
        Serial.println("\nDS3231 interrupt fired!\n");
        rtc.clearAlarm(1);
    }


    // print current time
    char date[10] = "hh:mm:ss";
    rtc.now().toString(date);
    Serial.print(date);

    static uint32_t lastEdgeCount = 0;
    uint32_t edgeCount = sqwFallingEdges;

    // Sample the pin and edge counter frequently enough to avoid aliasing with 1 Hz.
    Serial.print("-> SQW: ");
    Serial.print(digitalRead(CLOCK_INTERRUPT_PIN));
    Serial.print(" FallingEdges: ");
    Serial.print(edgeCount);
    Serial.print(" (+");
    Serial.print(edgeCount - lastEdgeCount);
    Serial.print(")");

    Serial.println();
    lastEdgeCount = edgeCount;

    delay(250);
}
