#include <Arduino.h>
#include <time.h>

// Convert compile-time macros into a tm structure
void getCompileTime(struct tm &tm) {
    // __DATE__ format: "Feb 20 2026"
    // __TIME__ format: "17:30:00"

    const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
    char monthStr[4];
    int day, year, hour, minute, second;

    sscanf(__DATE__, "%3s %d %d", monthStr, &day, &year);
    sscanf(__TIME__, "%d:%d:%d", &hour, &minute, &second);

    int month = (strstr(months, monthStr) - months) / 3;

    tm.tm_year = year - 1900;
    tm.tm_mon  = month;
    tm.tm_mday = day;
    tm.tm_hour = hour + 5;
    tm.tm_min  = minute;
    tm.tm_sec  = second;
    tm.tm_isdst = -1;  // Let system determine DST
}

void setup() {
    Serial.begin(115200);
    delay(500);

    Serial.println("\nStarting compile-time clock…");

    struct tm compileTM;
    getCompileTime(compileTM);

    // Convert to time_t
    time_t t = mktime(&compileTM);

    // Set timezone to Eastern Time with automatic DST
    // EST: UTC-5, EDT: UTC-4
    setenv("TZ", "EST5EDT,M3.2.0/2,M11.1.0/2", 1);
    tzset();

    // Apply the time
    struct timeval now = { .tv_sec = t };
    settimeofday(&now, nullptr);

    Serial.println("Clock initialized from compile time.");
}

void loop() {
    time_t now = time(nullptr);
    struct tm *local = localtime(&now);

    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S %Z", local);

    Serial.println(buf);
    delay(1000);
}