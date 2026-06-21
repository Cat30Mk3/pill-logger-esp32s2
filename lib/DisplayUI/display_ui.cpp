#include <Arduino.h>
#include <U8g2lib.h>
#include <MUIU8g2.h>
#include <Wire.h>
#include <string.h>
#include <stdio.h>
#include "globals.h"
#include "display_ui.h"
#include "data_store.h"
#include <WiFi.h>
#include <WiFiMulti.h>
#include "time_logic.h"
#include "core_system.h"
#include "battery.h"

// ============ DISPLAY STATE ============
static uint8_t currentScreenIndex = 0;
static const uint8_t MAX_SCREENS = 7;
static bool popupActive = false;
static char popupLine1[24] = {0};
static char popupLine2[24] = {0};
static char ssid[32] = {0};
static char rssi[16] = {0};

static void drawCentered(const char *str, int y, const uint8_t *font = u8g2_font_ncenB08_tr)
{
    u8g2.setFont(font);
    int textWidth = u8g2.getStrWidth(str);
    int oledWidth = u8g2.getDisplayWidth();
    int x = (oledWidth - textWidth) / 2;
    u8g2.drawStr(x, y, str);
}



static void drawCenteredTextBox(const char *str, int topLeftX, int topLeftY, int bottomRightX, int bottomRightY, const uint8_t *font = u8g2_font_ncenB08_tr)
{
 // positions text in the center of a defined box area, allowing for multi-line text to be centered within the box

    u8g2.setFont(font);
    int textWidth = u8g2.getStrWidth(str);
    int oledWidth = u8g2.getDisplayWidth();
    int boxWidth = bottomRightX - topLeftX;
    int boxHeight = bottomRightY - topLeftY;
    int textHeight = u8g2.getMaxCharHeight();
    int y = topLeftY + (boxHeight / 2) + (textHeight /2) - 2; // adjust y to account for font baseline, empirically determined to be about 2 pixels for this font   
    int x = topLeftX + (boxWidth / 2) - (textWidth / 2)- 1; // adjust x by 1 pixel for better visual centering, empirically determined  

    u8g2.setDrawColor(1); // Invert text color for contrast -- not sure why this is necessary on the 128x32 but it is for readability
    u8g2.drawStr(x, y, str);
    u8g2.setDrawColor(1); // Restore normal draw color
}



static void formatTimestampYmd(const time_t ts, char *out, size_t outSize)
{
    struct tm lt = getLocalTimestampTime(ts);
    strftime(out, outSize, "%Y/%m/%d", &lt);
}

static void formatTimestampYyMdHms(const time_t ts, char *out, size_t outSize)
{
    struct tm lt = getLocalTimestampTime(ts);
    strftime(out, outSize, "%y/%m/%d %H:%M:%S", &lt);
}

static void formatTimestampDebug(const time_t ts, char *out, size_t outSize)
{
    if (ts == 0)
    {
        snprintf(out, outSize, "0");
        return;
    }

    struct tm lt = getLocalTimestampTime(ts);
    strftime(out, outSize, "%Y-%m-%d %H:%M:%S %Z", &lt);
}

// ============ INITIALIZATION ============
void initDisplay()
{
    u8g2.begin();
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(20, 32, "Initializing...");
    u8g2.sendBuffer();
    DBG_PRINTLN("initDisplay: Display initialized");
}

// ============ SETUP SCREEN RENDERING ============
void renderSetupScreen(uint8_t setupState)
{
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    
    switch (setupState) {
        case SETUP_STATE_SERIAL_WAIT:
            drawCentered("Waiting for", 12, u8g2_font_6x10_tf);
            drawCentered("Serial CDC...", 22, u8g2_font_6x10_tf);
            break;
        case SETUP_INITIALIZATION:
            drawCentered("Setup", 12, u8g2_font_6x10_tf);
            drawCentered("Initializing...", 22, u8g2_font_6x10_tf);
            break;
        case SETUP_STATE_WIFI_CONNECTING:
            drawCentered("WiFi", 12, u8g2_font_6x10_tf);
            drawCentered("Connecting...", 22, u8g2_font_6x10_tf);
            break;
        case SETUP_STATE_NTP_SYNCING:
            drawCentered("NTP Sync", 12, u8g2_font_6x10_tf);
            drawCentered("In Progress...", 22, u8g2_font_6x10_tf);
            break;
        case SETUP_STATE_READY:
            drawCentered("Setup", 12, u8g2_font_6x10_tf);
            drawCentered("Complete!", 22, u8g2_font_6x10_tf);
            break;
        default:
            drawCentered("Unknown", 12, u8g2_font_6x10_tf);
            drawCentered("State", 22, u8g2_font_6x10_tf);
            break;
    }
    
    u8g2.sendBuffer();
}

// ============ SCREEN RENDERING ============
void renderScreen(uint8_t index)
{
    static uint32_t lastClockRefreshMs = 0;
    static char clockTimeLine[20] = "";
    static char clockDateLine[20] = "";

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);

    if (popupActive)
    {
        drawCentered(popupLine1, 12, u8g2_font_6x10_tf);
        drawCentered(popupLine2, 26, u8g2_font_6x10_tf);
        u8g2.drawFrame(0, 0, 128, 32);
        u8g2.sendBuffer();
        return;
    }

    uint8_t screen = index % MAX_SCREENS;

    switch (screen)
    {
    case 0:
    {
        char tsLine1[20];
        char tsLine2[20];
        char countLine[20];
        struct tm lp = getLocalTimestampTime((time_t)pdata.last_pill_taken_timestamp);

        strftime(tsLine1, sizeof(tsLine1), "%a", &lp);
        strftime(tsLine2, sizeof(tsLine2), "%H:%M", &lp);

        snprintf(countLine, sizeof(countLine), "%u", (unsigned)pdata.pills_taken_today_count);

        //drawCentered("0-Pills Today", 10, u8g2_font_6x10_tf);
       // drawCentered(tsLine1, 21, u8g2_font_6x10_tf);
       // drawCentered(countLine, 31, u8g2_font_6x10_tf);

        drawCenteredTextBox("0-Pills Taken Today", 0, 0, 128, 10, u8g2_font_6x10_tf); // title on full width with smaller font
        drawCenteredTextBox(tsLine1, 64, 10, 128, 20, u8g2_font_6x10_tf); // day on right half of screen with smaller font
        drawCenteredTextBox(tsLine2, 64, 20, 128, 32, u8g2_font_6x10_tf);   // timestamp on right half of screen with smaller font
        drawCenteredTextBox(countLine, 0, 10, 64, 32, u8g2_font_8x13B_tn);  //counter on left half of screen with larger font

        break;
    }

    case 1:
    {
       
        // Keep main loop responsive while updating clock display values at 1 Hz.
        if ((millis() - lastClockRefreshMs) >= 1000 || clockTimeLine[0] == '\0')
        {
            struct tm nowLocal = getLocalTime_Tm();
            strftime(clockTimeLine, sizeof(clockTimeLine), "%H:%M:%S %Z", &nowLocal);
            strftime(clockDateLine, sizeof(clockDateLine), "%a %b %d", &nowLocal);
            lastClockRefreshMs = millis();
        }

        drawCentered("1-Clock", 10, u8g2_font_6x10_tf);
        drawCentered(clockTimeLine, 20, u8g2_font_6x10_tf);
        drawCentered(clockDateLine, 30, u8g2_font_6x10_tf);

        break;
    }

    case 2:
    {
        char refillLast[16];
        char refillNext[16];
        char line1[24];
        char line2[24];

        formatTimestampYmd((time_t)pdata.Rx_last_refill_date, refillLast, sizeof(refillLast));
        formatTimestampYmd((time_t)pdata.Rx_next_refill_date, refillNext, sizeof(refillNext));

        snprintf(line1, sizeof(line1), "Last: %s", &refillLast[2]);
        snprintf(line2, sizeof(line2), "Next: %s", &refillNext[2]);

        drawCentered("2-Rx Refill", 10, u8g2_font_6x10_tf);
        drawCentered(line1, 20, u8g2_font_5x8_tr);
        drawCentered(line2, 30, u8g2_font_5x8_tr);
        break;
    }

    case 3:
    {
        char countLine[16];
        char depleted[16];
        char tsLine1[20];
        char line1[24];



        formatTimestampYmd((time_t)pdata.pills_depleted_date, tsLine1, sizeof(tsLine1));

        snprintf(countLine, sizeof(countLine), "%u", (unsigned)pdata.pill_remaining_count);
        snprintf(depleted, sizeof(depleted), "%s", "Depleted");
        snprintf(line1, sizeof(tsLine1), "%s", &tsLine1[2]);  //skips th "20"of the year for more compact display need & in this case to avoid overwriting tsLine1 which is used in the next screen for NTP sync timestamp display

        //drawCentered("3-Pills Left", 10, u8g2_font_6x10_tf);
        //drawCentered(line1, 20, u8g2_font_5x8_tr);
        //drawCentered(line2, 30, u8g2_font_5x8_tr);

        drawCenteredTextBox("3-Pills Left", 0, 0, 128, 10, u8g2_font_6x10_tf); // title on full width with smaller font
        drawCenteredTextBox(depleted, 64, 10, 128, 20, u8g2_font_6x10_tf); // day on right half of screen with smaller font
        drawCenteredTextBox(line1, 64, 20, 128, 32, u8g2_font_6x10_tf);   // timestamp on right half of screen with smaller font
        drawCenteredTextBox(countLine, 0, 10, 64, 32, u8g2_font_8x13B_tn);  //counter on left half of screen with larger font
        
        break;
    }


    case 4:
    {
        static uint32_t lastBatteryRefreshMs = 0;
        static float display_voltage = 0.0f;
        static uint8_t display_soc = 0;
        static char display_status[32] = "";
        
        // On first call or battery test mode, update immediately
        // Otherwise, refresh every 1000ms
        if (BATTERY_TEST_MODE_ENABLE || display_status[0] == '\0' || (millis() - lastBatteryRefreshMs) >= 1000) {
            readBatteryVoltage();
            display_voltage = getBatteryVoltage();
            display_soc = getBatterySoc();
            strncpy(display_status, getBatteryStatus(), sizeof(display_status) - 1);
            display_status[sizeof(display_status) - 1] = '\0';
            lastBatteryRefreshMs = millis();
        }
        
        char vLine[20];
        char pLine[40];
        
        // Format based on mode
        if (isBatteryError()) {
            // Error mode
            snprintf(vLine, sizeof(vLine), "%.2f V", display_voltage);
            snprintf(pLine, sizeof(pLine), "%s", display_status);
        } else if (isUsbPowered()) {
            // USB mode - no percentage shown
            snprintf(vLine, sizeof(vLine), "%.2f V", display_voltage);
            snprintf(pLine, sizeof(pLine), "%s", display_status);
        } else {
            // Normal battery mode
            snprintf(vLine, sizeof(vLine), "%.2f V  %u%%", display_voltage, display_soc);
            snprintf(pLine, sizeof(pLine), "%s", display_status);
        }
        
        drawCentered("4-Batt Stat", 10, u8g2_font_6x10_tf);
        drawCentered(vLine, 20, u8g2_font_6x10_tf);
        drawCentered(pLine, 30, u8g2_font_5x8_tr);
        break;
    }

    case 5:
    {
        char syncLine[20];
        char statusLine[28];
        uint8_t failureIdx = rtc_fast_state.last_ntp_failure_cause;
        if (failureIdx > NTP_INVALID)
        {
            failureIdx = NTP_INVALID;
        }
        formatTimestampYyMdHms((time_t)rtc_fast_state.last_ntp_sync_timestamp, syncLine, sizeof(syncLine));
        snprintf(statusLine, sizeof(statusLine), "%d-%s retries %u", rtc_fast_state.last_ntp_sync_reason, ntp_failure_cause_str[failureIdx], (unsigned)rtc_fast_state.ntp_retry_count);

        drawCentered("5-NTP Stat", 10, u8g2_font_6x10_tf);
        drawCentered(syncLine, 20, u8g2_font_6x10_tf);
        drawCentered(statusLine, 30, u8g2_font_6x10_tf);
        break;
    }

        case 6:
    {
        snprintf(ssid, sizeof(ssid), "%s", rtc_fast_state.last_wifi_ssid);
        snprintf(rssi, sizeof(rssi), "RSSI: %d dBm", rtc_fast_state.last_wifi_rssi);

        drawCentered("6-WiFi", 10, u8g2_font_6x10_tf);
        drawCentered(ssid, 20, u8g2_font_6x10_tf);
        drawCentered(rssi, 30, u8g2_font_6x10_tf);
        break;
    }

    default:
        drawCentered("Unknown Screen", 20, u8g2_font_6x10_tf);
        break;
    }

    u8g2.drawFrame(0, 0, 128, 32);

    u8g2.sendBuffer();
}

// ============ SCREEN NAVIGATION ============
void nextScreen()
{
    currentScreenIndex = (currentScreenIndex + 1) % MAX_SCREENS;
}

uint8_t getScreenIndex()
{
    return currentScreenIndex;
}

void setScreenIndex(uint8_t index)
{
    currentScreenIndex = index % MAX_SCREENS;
}

void showConfirmationPopup(const char *line1, const char *line2)
{
    if (line1 == NULL)
    {
        line1 = "";
    }
    if (line2 == NULL)
    {
        line2 = "";
    }

    strncpy(popupLine1, line1, sizeof(popupLine1) - 1);
    popupLine1[sizeof(popupLine1) - 1] = '\0';
    strncpy(popupLine2, line2, sizeof(popupLine2) - 1);
    popupLine2[sizeof(popupLine2) - 1] = '\0';
    popupActive = true;
}

void clearConfirmationPopup()
{
    popupActive = false;
    popupLine1[0] = '\0';
    popupLine2[0] = '\0';
}

bool isConfirmationPopupActive()
{
    return popupActive;
}

void checkMuiEntryStub() {} // legacy stub - kept so archive files compile

// ============ MUI EDIT MODE (Milestone 4) ============
static MUIU8G2 g_mui;
static bool g_mui_ready = false;

// Stage 3 Rx staged editor values (applied to pdata in Stage 5)
static uint8_t rx_edit_year_yy = 26;
static uint8_t rx_edit_month = 1;
static uint8_t rx_edit_day = 1;
static uint8_t rx_edit_count_100 = 0;
static uint8_t rx_edit_count_10 = 0;
static uint8_t rx_edit_count_1 = 0;
static uint8_t rx_edit_ppd = 0;

// Stage 4 Pills Remaining staged editor values (applied to pdata in Stage 5)
static uint8_t pills_edit_100 = 0;
static uint8_t pills_edit_10 = 0;
static uint8_t pills_edit_1 = 0;

// ============ NTP SYNC STATUS RENDERER ============
static void renderNtpSyncStatus(const char *line1, const char *line2)
{
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    drawCentered("Manual NTP Sync", 10, u8g2_font_6x10_tf);
    drawCentered(line1, 20, u8g2_font_6x10_tf);
    drawCentered(line2, 30, u8g2_font_5x8_tr);
    u8g2.drawFrame(0, 0, 128, 32);
    u8g2.sendBuffer();
}

// Forward declarations for MUI callbacks
static uint8_t muif_exit_edit_now(mui_t *ui, uint8_t msg);
static uint8_t muif_ntp_sync_action(mui_t *ui, uint8_t msg);

// ============ NTP MANUAL SYNC MUI (triggered from screen 5, PB_RIGHT) ============
static muif_t ntp_muif_list[] = {
    MUIF_U8G2_FONT_STYLE(0, u8g2_font_5x8_tr),
    MUIF_U8G2_LABEL(),
    MUIF_RO("GP", mui_u8g2_goto_data),
    MUIF_BUTTON("GC", mui_u8g2_goto_form_w1_pi),
    MUIF_RO("X0", muif_exit_edit_now),   // Exit: leave NTP menu
    MUIF_RO("NS", muif_ntp_sync_action), // Sync Now: trigger manual NTP sync
};

static fds_t ntp_fds_data[] =
    MUI_FORM(1)
        MUI_STYLE(0)
            MUI_LABEL(2, 8, "NTP Options")
                MUI_DATA("GP",
                         MUI_90 "Exit|" MUI_2 "Sync Now")
                    MUI_XYA("GC", 2, 20, 0)
                        MUI_XYA("GC", 2, 30, 1)

                            MUI_FORM(2)
                                MUI_STYLE(0)
                                    MUI_XY("NS", 0, 0)

                                        MUI_FORM(90)
                                            MUI_STYLE(0)
                                                MUI_XY("X0", 0, 0);

static muif_t edit_muif_list[] = {
    MUIF_U8G2_FONT_STYLE(0, u8g2_font_5x8_tr),
    MUIF_U8G2_LABEL(),

    // Scrollable jump-table data + button rows
    MUIF_RO("GP", mui_u8g2_goto_data),
    MUIF_BUTTON("GC", mui_u8g2_goto_form_w1_pi),

    // Rx numeric edit fields
    MUIF_U8G2_U8_MIN_MAX("YR", &rx_edit_year_yy, 26, 36, mui_u8g2_u8_min_max_wm_mud_pi),
    MUIF_U8G2_U8_MIN_MAX("MO", &rx_edit_month, 1, 12, mui_u8g2_u8_min_max_wm_mud_pi),
    MUIF_U8G2_U8_MIN_MAX("DY", &rx_edit_day, 1, 31, mui_u8g2_u8_min_max_wm_mud_pi),
    MUIF_U8G2_U8_MIN_MAX("H1", &rx_edit_count_100, 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),
    MUIF_U8G2_U8_MIN_MAX("T1", &rx_edit_count_10, 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),
    MUIF_U8G2_U8_MIN_MAX("O1", &rx_edit_count_1, 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),
    MUIF_U8G2_U8_MIN_MAX("PD", &rx_edit_ppd, 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),

    // Pills remaining numeric edit fields
    MUIF_U8G2_U8_MIN_MAX("P1", &pills_edit_100, 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),
    MUIF_U8G2_U8_MIN_MAX("P2", &pills_edit_10, 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),
    MUIF_U8G2_U8_MIN_MAX("P3", &pills_edit_1, 0, 9, mui_u8g2_u8_min_max_wm_mud_pi),

    // Exit buttons back to owning menu
    MUIF_BUTTON("BM", mui_u8g2_btn_goto_w1_fi),

    // Immediate menu-exit trigger form element
    MUIF_RO("X0", muif_exit_edit_now),
};

static fds_t edit_fds_data[] =
    MUI_FORM(20)
        MUI_STYLE(0)
            MUI_LABEL(2, 8, "Rx Edit Menu")
                MUI_DATA("GP",
                         MUI_90 "Exit|" MUI_21 "Rx_date|" MUI_22 "Rx_count|" MUI_23 "Rx_pill_day")
                    MUI_XYA("GC", 2, 20, 0)
                        MUI_XYA("GC", 2, 30, 1)

                            MUI_FORM(21)
                                MUI_STYLE(0)
                                    MUI_LABEL(2, 8, "Rx Date")
                                        MUI_XYAT("BM", 2, 18, 20, "Exit")
                                            MUI_LABEL(52, 18, "Y  M  D")
                                                MUI_XY("YR", 52, 30)
                                                    MUI_XY("MO", 72, 30)
                                                        MUI_XY("DY", 92, 30)

                                                            MUI_FORM(22)
                                                                MUI_STYLE(0)
                                                                    MUI_LABEL(2, 8, "Rx Count")
                                                                        MUI_XYAT("BM", 2, 18, 20, "Exit")
                                                                            MUI_LABEL(52, 18, "100 10 1")
                                                                                MUI_XY("H1", 52, 30)
                                                                                    MUI_XY("T1", 72, 30)
                                                                                        MUI_XY("O1", 92, 30)

                                                                                            MUI_FORM(23)
                                                                                                MUI_STYLE(0)
                                                                                                    MUI_LABEL(2, 8, "Rx Pill/Day")
                                                                                                        MUI_XYAT("BM", 2, 18, 20, "Exit")
                                                                                                            MUI_LABEL(52, 18, "Value")
                                                                                                                MUI_XY("PD", 72, 30)

                                                                                                                    MUI_FORM(30)
                                                                                                                        MUI_STYLE(0)
                                                                                                                            MUI_LABEL(2, 8, "Pills Edit Menu")
                                                                                                                                MUI_DATA("GP",
                                                                                                                                         MUI_90 "Exit|" MUI_31 "pills_remaining")
                                                                                                                                    MUI_XYA("GC", 2, 20, 0)
                                                                                                                                        MUI_XYA("GC", 2, 30, 1)

                                                                                                                                            MUI_FORM(31)
                                                                                                                                                MUI_STYLE(0)
                                                                                                                                                    MUI_LABEL(2, 8, "Pills Remaining")
                                                                                                                                                        MUI_XYAT("BM", 2, 18, 30, "Exit")
                                                                                                                                                            MUI_LABEL(52, 18, "100 10 1")
                                                                                                                                                                MUI_XY("P1", 52, 30)
                                                                                                                                                                    MUI_XY("P2", 72, 30)
                                                                                                                                                                        MUI_XY("P3", 92, 30)

                                                                                                                                                                            MUI_FORM(90)
                                                                                                                                                                                MUI_STYLE(0)
                                                                                                                                                                                    MUI_XY("X0", 0, 0);

static void loadRxEditStagingFromPersistent()
{
    struct tm rxLocal = getLocalTimestampTime((time_t)pdata.Rx_last_refill_date);
    uint8_t yy = (uint8_t)(rxLocal.tm_year % 100);
    if (yy < 26)
        yy = 26;
    if (yy > 36)
        yy = 36;

    rx_edit_year_yy = yy;
    rx_edit_month = (uint8_t)(rxLocal.tm_mon + 1);
    if (rx_edit_month < 1)
        rx_edit_month = 1;
    if (rx_edit_month > 12)
        rx_edit_month = 12;

    rx_edit_day = (uint8_t)rxLocal.tm_mday;
    if (rx_edit_day < 1)
        rx_edit_day = 1;
    if (rx_edit_day > 31)
        rx_edit_day = 31;

    uint16_t count = pdata.Rx_dispensed_pill_count;
    if (count > 999)
        count = 999;
    rx_edit_count_100 = (uint8_t)(count / 100);
    rx_edit_count_10 = (uint8_t)((count / 10) % 10);
    rx_edit_count_1 = (uint8_t)(count % 10);

    rx_edit_ppd = pdata.Rx_pills_per_day;
    if (rx_edit_ppd > 9)
        rx_edit_ppd = 9;
}

static void loadPillsEditStagingFromPersistent()
{
    uint16_t count = pdata.pill_remaining_count;
    if (count > 999)
        count = 999;
    pills_edit_100 = (uint8_t)(count / 100);
    pills_edit_10 = (uint8_t)((count / 10) % 10);
    pills_edit_1 = (uint8_t)(count % 10);
}

static bool isLeapYear(int fullYear)
{
    return ((fullYear % 4 == 0) && (fullYear % 100 != 0)) || (fullYear % 400 == 0);
}

static uint8_t daysInMonth(uint8_t month, int fullYear)
{
    if (month < 1 || month > 12)
    {
        return 31;
    }

    static const uint8_t days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint8_t d = days[month - 1];
    if (month == 2 && isLeapYear(fullYear))
    {
        d = 29;
    }
    return d;
}

static uint32_t buildLocalYmdTimestamp(uint8_t yy, uint8_t month, uint8_t day)
{
    uint8_t inputMonth = month;
    uint8_t inputDay = day;
    int fullYear = 2000 + (int)yy;
    if (month < 1)
        month = 1;
    if (month > 12)
        month = 12;

    uint8_t maxDay = daysInMonth(month, fullYear);
    if (day < 1)
        day = 1;
    if (day > maxDay)
        day = maxDay;

    setenv("TZ", TIMEZONE_RULE, 1);
    tzset();

    struct tm tmLocal = {};
    tmLocal.tm_year = fullYear - 1900;
    tmLocal.tm_mon = month - 1;
    tmLocal.tm_mday = day;
    tmLocal.tm_hour = 0;
    tmLocal.tm_min = 0;
    tmLocal.tm_sec = 0;
    tmLocal.tm_isdst = -1;

    time_t ts = mktime(&tmLocal);
    if (ts < 0)
    {
        DBG_PRINTF("[EDIT][RX] buildLocalYmdTimestamp failed for input=%02u/%02u/%02u\n",
                   (unsigned)yy,
                   (unsigned)inputMonth,
                   (unsigned)inputDay);
        return 0;
    }

    if (month != inputMonth || day != inputDay)
    {
        DBG_PRINTF("[EDIT][RX] date clamped input=%02u/%02u/%02u -> applied=%02u/%02u/%02u\n",
                   (unsigned)yy,
                   (unsigned)inputMonth,
                   (unsigned)inputDay,
                   (unsigned)yy,
                   (unsigned)month,
                   (unsigned)day);
    }
    return (uint32_t)ts;
}

static void commitRxEditsToPersistent()
{
    uint16_t rxCount = (uint16_t)(rx_edit_count_100 * 100u + rx_edit_count_10 * 10u + rx_edit_count_1);
    uint8_t rxPpd = rx_edit_ppd;
    uint32_t previousLast = pdata.Rx_last_refill_date;
    uint16_t previousCount = pdata.Rx_dispensed_pill_count;
    uint8_t previousPpd = pdata.Rx_pills_per_day;
    uint32_t previousNext = pdata.Rx_next_refill_date;
    char prevLastStr[32];
    char prevNextStr[32];
    char newLastStr[32];
    char newNextStr[32];

    pdata.Rx_last_refill_date = buildLocalYmdTimestamp(rx_edit_year_yy, rx_edit_month, rx_edit_day);
    pdata.Rx_dispensed_pill_count = rxCount;
    pdata.Rx_pills_per_day = rxPpd;

    if (rxPpd > 0)
    {
        uint32_t daysToRefill = (uint32_t)(rxCount / rxPpd);
        pdata.Rx_next_refill_date = pdata.Rx_last_refill_date + (daysToRefill * 86400UL);
    }
    else
    {
        pdata.Rx_next_refill_date = pdata.Rx_last_refill_date;
    }

    formatTimestampDebug(previousLast, prevLastStr, sizeof(prevLastStr));
    formatTimestampDebug(previousNext, prevNextStr, sizeof(prevNextStr));
    formatTimestampDebug(pdata.Rx_last_refill_date, newLastStr, sizeof(newLastStr));
    formatTimestampDebug(pdata.Rx_next_refill_date, newNextStr, sizeof(newNextStr));

    DBG_PRINTF("[EDIT][RX] commit date=%02u/%02u/%02u count=%u ppd=%u\n",
               (unsigned)rx_edit_year_yy,
               (unsigned)rx_edit_month,
               (unsigned)rx_edit_day,
               (unsigned)rxCount,
               (unsigned)rxPpd);
    DBG_PRINTF("[EDIT][RX] last: %s -> %s\n", prevLastStr, newLastStr);
    DBG_PRINTF("[EDIT][RX] next: %s -> %s\n", prevNextStr, newNextStr);
    DBG_PRINTF("[EDIT][RX] count: %u -> %u, ppd: %u -> %u\n",
               (unsigned)previousCount,
               (unsigned)pdata.Rx_dispensed_pill_count,
               (unsigned)previousPpd,
               (unsigned)pdata.Rx_pills_per_day);
    if (rxPpd > 0)
    {
        DBG_PRINTF("[EDIT][RX] daysToRefill=%lu (integer division of %u/%u)\n",
                   (unsigned long)(rxCount / rxPpd),
                   (unsigned)rxCount,
                   (unsigned)rxPpd);
    }
    else
    {
        DBG_PRINTLN("[EDIT][RX] ppd=0 so next refill date is pinned to last refill date");
    }
}

static void commitPillsEditsToPersistent()
{
    uint16_t pills = (uint16_t)(pills_edit_100 * 100u + pills_edit_10 * 10u + pills_edit_1);
    uint16_t previousPills = pdata.pill_remaining_count;
    pdata.pill_remaining_count = pills;
    DBG_PRINTF("[EDIT][PILLS] remaining count: %u -> %u\n",
               (unsigned)previousPills,
               (unsigned)pdata.pill_remaining_count);
}

static void recalcPillsDepletedDateFromCurrent()
{
    uint32_t nowUtc = getUtcTime();
    uint8_t ppd = pdata.Rx_pills_per_day;
    uint16_t remaining = pdata.pill_remaining_count;
    uint32_t previous = pdata.pills_depleted_date;

    if (ppd > 0)
    {
        uint32_t daysToDepletion = (uint32_t)(remaining / ppd);
        pdata.pills_depleted_date = nowUtc + (daysToDepletion * 86400UL);
        DBG_PRINTF("[EDIT][DEPL] recalculated using now=%lu remaining=%u ppd=%u days=%lu\n",
                   (unsigned long)nowUtc,
                   (unsigned)remaining,
                   (unsigned)ppd,
                   (unsigned long)daysToDepletion);
    }
    else
    {
        // Guard divide-by-zero: keep depletion horizon at current date when ppd is unknown.
        pdata.pills_depleted_date = nowUtc;
        DBG_PRINTF("[EDIT][DEPL] ppd=0, set depleted date to now=%lu\n", (unsigned long)nowUtc);
    }

    {
        char prevStr[32];
        char newStr[32];
        formatTimestampDebug(previous, prevStr, sizeof(prevStr));
        formatTimestampDebug(pdata.pills_depleted_date, newStr, sizeof(newStr));
        DBG_PRINTF("[EDIT][DEPL] date: %s -> %s\n", prevStr, newStr);
    }
}

static void savePersistentDataWithRetry()
{
    const uint8_t maxAttempts = 2;
    for (uint8_t attempt = 1; attempt <= maxAttempts; ++attempt)
    {
        savePersistentData();
        DBG_PRINTF("[EDIT] savePersistentData attempt %u/%u\n", (unsigned)attempt, (unsigned)maxAttempts);
    }
}

static uint8_t muif_exit_edit_now(mui_t *ui, uint8_t msg)
{
    if (msg == MUIF_MSG_FORM_START)
    {
        mui_LeaveForm(ui);
        exitEditMode();
        return 1;
    }
    return 0;
}

static uint8_t muif_ntp_sync_action(mui_t *ui, uint8_t msg)
{
    if (msg == MUIF_MSG_FORM_START)
    {
        mui_LeaveForm(ui);
        lastActivityTime = millis(); // Suppress inactivity timeout during sync

        // Phase 1: WiFi connect
        renderNtpSyncStatus("Connecting", "WiFi...");
        initWiFi(); // Ensure wifiMulti has AP credentials loaded
        bool wifiOk = wifi_connect_for_ntp();
        if (!wifiOk)
        {
            renderNtpSyncStatus("WiFi", "Connect Failed");
            rtc_fast_state.last_ntp_sync_reason = 4; // 4 = manual sync from UI
            rtc_save();
            delay(2000);
            lastActivityTime = millis();
            exitEditMode();
            return 1;
        }

        // Phase 2: NTP sync
        renderNtpSyncStatus("NTP Sync", "In Progress...");
        bool ntpOk = ntp_sync_only();
        rtc_fast_state.last_ntp_sync_reason = 4; // 4 = manual sync from UI
        rtc_save();

        if (ntpOk)
        {
            renderNtpSyncStatus("NTP Sync", "Success!");
        }
        else
        {
            renderNtpSyncStatus("NTP Sync", "Failed!");
        }

        delay(2000);
        lastActivityTime = millis();
        exitEditMode();
        return 1;
    }
    return 0;
}

void enterEditMode(EditState state)
{
    g_edit_state = state;
    g_edit_mode_active = true;
    clearAllNavPressFlags();
    lastActivityTime = millis();

    if (state == EDIT_RX_MENU)
    {
        loadRxEditStagingFromPersistent();
        g_mui.begin(u8g2, edit_fds_data, edit_muif_list, sizeof(edit_muif_list) / sizeof(muif_t));
        g_mui.gotoForm(20, 0); // initial focus on Exit in the menu list
        g_mui_ready = true;
    }
    else if (state == EDIT_PILLS_MENU)
    {
        loadPillsEditStagingFromPersistent();
        g_mui.begin(u8g2, edit_fds_data, edit_muif_list, sizeof(edit_muif_list) / sizeof(muif_t));
        g_mui.gotoForm(30, 0); // initial focus on Exit in the menu list
        g_mui_ready = true;
    }
    else if (state == EDIT_NTP_MENU)
    {
        g_mui.begin(u8g2, ntp_fds_data, ntp_muif_list, sizeof(ntp_muif_list) / sizeof(muif_t));
        g_mui.gotoForm(1, 0); // initial focus on Exit (first item)
        g_mui_ready = true;
    }
    else
    {
        g_mui_ready = false;
    }

    DBG_PRINTF("[EDIT] enterEditMode state=%d\n", (int)state);
}

void exitEditMode(bool resetActivityTimer)
{
    EditState exitingState = g_edit_state;

    if (exitingState == EDIT_RX_MENU || exitingState == EDIT_RX_DATE || exitingState == EDIT_RX_COUNT || exitingState == EDIT_RX_PPD)
    {
        commitRxEditsToPersistent();
        recalcPillsDepletedDateFromCurrent();
        savePersistentDataWithRetry();
    }
    else if (exitingState == EDIT_PILLS_MENU || exitingState == EDIT_PILLS_REM)
    {
        commitPillsEditsToPersistent();
        recalcPillsDepletedDateFromCurrent();
        savePersistentDataWithRetry();
    }

    clearAllNavPressFlags();
    g_edit_mode_active = false;
    g_edit_state = EDIT_NONE;
    g_mui_ready = false;
    if (resetActivityTimer)
    {
        lastActivityTime = millis();
    }
    DBG_PRINTF("[EDIT] exitEditMode state=%d resetActivityTimer=%s\n",
               (int)exitingState,
               resetActivityTimer ? "true" : "false");
}

void checkMuiEntry()
{
    if (isRightPress())
    {
        uint8_t screen = getScreenIndex();
        if (screen == 2)
        {
            enterEditMode(EDIT_RX_MENU);
            return;
        }
        if (screen == 3)
        {
            enterEditMode(EDIT_PILLS_MENU);
            return;
        }
        if (screen == 5)
        {
            enterEditMode(EDIT_NTP_MENU);
            return;
        }
    }

    isLeftPress();
    isSelectPress();
}

void processMuiInput()
{
    if (g_mui_ready)
    {
        if (isSelectPress())
        {
            g_mui.sendSelect();
        }
        if (isRightPress())
        {
            g_mui.nextField();
        }
        if (isLeftPress())
        {
            g_mui.prevField();
        }
        return;
    }

    isSelectPress();
    isLeftPress();
    isRightPress();
}

void renderEditScreen()
{
    if (g_mui_ready)
    {
        if (!g_mui.isFormActive())
        {
            // Handles menu Exit path in case form became inactive.
            exitEditMode();
            return;
        }

        u8g2.clearBuffer();
        g_mui.draw();
        u8g2.drawFrame(0, 0, 128, 32);
        u8g2.sendBuffer();
        return;
    }

    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    drawCentered("Edit Mode", 12, u8g2_font_6x10_tf);
    drawCentered("MUI not active", 26, u8g2_font_6x10_tf);
    u8g2.drawFrame(0, 0, 128, 32);
    u8g2.sendBuffer();
}
