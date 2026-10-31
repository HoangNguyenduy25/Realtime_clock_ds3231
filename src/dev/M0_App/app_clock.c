#include "app_clock.h"

#include <stddef.h>
#include "../M1_Sys/Scheduler/scheduler.h"
#include "../M1_Sys/SystemData/system_data.h"
#include "../M2_BSP/DS3231/bsp_ds3231.h"

/**
 * Co-routine states for non-blocking DS3231 RTC acquisition & alarm checking.
 */
typedef enum
{
    APP_CLOCK_CO_WAIT_INTERVAL = 0,
    APP_CLOCK_CO_READ_DATETIME,
    APP_CLOCK_CO_CHECK_ALARM
} AppClock_CoState_t;

static AppClock_CoState_t s_clock_co_state = APP_CLOCK_CO_WAIT_INTERVAL;
static uint32_t           s_last_rtc_poll_ms = 0UL;
static bool               s_force_rtc_sync = true;

/* String Parsing & Formatting Helpers */
static bool app_clock_parse_2digits(const char *p, uint8_t *out_val)
{
    if (p == NULL || p[0] < '0' || p[0] > '9' || p[1] < '0' || p[1] > '9')
    {
        return false;
    }
    *out_val = (uint8_t)(((p[0] - '0') * 10) + (p[1] - '0'));
    return true;
}

static bool app_clock_parse_4digits(const char *p, uint16_t *out_val)
{
    uint8_t i;
    uint16_t val = 0U;

    if (p == NULL)
    {
        return false;
    }
    for (i = 0U; i < 4U; i++)
    {
        if (p[i] < '0' || p[i] > '9')
        {
            return false;
        }
        val = (uint16_t)((val * 10U) + (uint16_t)(p[i] - '0'));
    }
    *out_val = val;
    return true;
}

static bool app_clock_is_alpha(char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

AppClock_Status_t AppClock_ParseTimeStr(const char *time_str,
                                        uint8_t *hours,
                                        uint8_t *minutes,
                                        uint8_t *seconds)
{
    uint8_t hh, mm, ss;

    if (time_str == NULL || hours == NULL || minutes == NULL || seconds == NULL)
    {
        return APP_CLOCK_ERR_BAD_FORMAT;
    }

    /* Exact format required by Section 5.2.10: "hh:mm:ss" (8 characters) */
    if (time_str[2] != ':' || time_str[5] != ':' || time_str[8] != '\0')
    {
        return APP_CLOCK_ERR_BAD_FORMAT;
    }

    if (!app_clock_parse_2digits(&time_str[0], &hh) ||
        !app_clock_parse_2digits(&time_str[3], &mm) ||
        !app_clock_parse_2digits(&time_str[6], &ss))
    {
        return APP_CLOCK_ERR_BAD_FORMAT;
    }

    if (!DS3231_IsValidTime(hh, mm, ss))
    {
        return APP_CLOCK_ERR_OUT_OF_RANGE;
    }

    *hours   = hh;
    *minutes = mm;
    *seconds = ss;
    return APP_CLOCK_OK;
}

AppClock_Status_t AppClock_ParseDateStr(const char *date_str,
                                        const char *dow_str,
                                        uint8_t *date,
                                        uint8_t *month,
                                        uint16_t *full_year,
                                        uint8_t *day)
{
    uint8_t  dd, mm, dow;
    uint16_t yyyy;

    if (date_str == NULL || dow_str == NULL ||
        date == NULL || month == NULL || full_year == NULL || day == NULL)
    {
        return APP_CLOCK_ERR_BAD_FORMAT;
    }

    /* Exact format required by Section 5.2.11: "dd/mm/yyyy" (10 characters) */
    if (date_str[2] != '/' || date_str[5] != '/' || date_str[10] != '\0')
    {
        return APP_CLOCK_ERR_BAD_FORMAT;
    }

    if (!app_clock_parse_2digits(&date_str[0], &dd) ||
        !app_clock_parse_2digits(&date_str[3], &mm) ||
        !app_clock_parse_4digits(&date_str[6], &yyyy))
    {
        return APP_CLOCK_ERR_BAD_FORMAT;
    }

    /* DOW must be 3 alphabetic characters; if valid letters but not MON..SUN -> E03 */
    if (dow_str[0] == '\0' || dow_str[1] == '\0' || dow_str[2] == '\0' || dow_str[3] != '\0' ||
        !app_clock_is_alpha(dow_str[0]) || !app_clock_is_alpha(dow_str[1]) || !app_clock_is_alpha(dow_str[2]))
    {
        return APP_CLOCK_ERR_BAD_FORMAT;
    }

    dow = DS3231_StringToDay(dow_str);
    if (!DS3231_IsValidDateRange(dd, mm, yyyy, dow))
    {
        return APP_CLOCK_ERR_OUT_OF_RANGE;
    }

    /* Check calendar existence (e.g., 31/02/2026 or 29/02 on non-leap year -> E07 INVALID DATE) */
    if (!DS3231_IsValidCalendarDate(dd, mm, yyyy))
    {
        return APP_CLOCK_ERR_INVALID_DATE;
    }

    *date      = dd;
    *month     = mm;
    *full_year = yyyy;
    *day       = dow;
    return APP_CLOCK_OK;
}

void AppClock_FormatTimeStr(uint8_t hours, uint8_t minutes, uint8_t seconds, char *out_buf)
{
    if (out_buf == NULL)
    {
        return;
    }
    out_buf[0] = (char)('0' + ((hours / 10U) % 10U));
    out_buf[1] = (char)('0' + (hours % 10U));
    out_buf[2] = ':';
    out_buf[3] = (char)('0' + ((minutes / 10U) % 10U));
    out_buf[4] = (char)('0' + (minutes % 10U));
    out_buf[5] = ':';
    out_buf[6] = (char)('0' + ((seconds / 10U) % 10U));
    out_buf[7] = (char)('0' + (seconds % 10U));
    out_buf[8] = '\0';
}

void AppClock_FormatDateStr(uint8_t date, uint8_t month, uint8_t year_2digit, char *out_buf)
{
    if (out_buf == NULL)
    {
        return;
    }
    out_buf[0]  = (char)('0' + ((date / 10U) % 10U));
    out_buf[1]  = (char)('0' + (date % 10U));
    out_buf[2]  = '/';
    out_buf[3]  = (char)('0' + ((month / 10U) % 10U));
    out_buf[4]  = (char)('0' + (month % 10U));
    out_buf[5]  = '/';
    out_buf[6]  = '2';
    out_buf[7]  = '0';
    out_buf[8]  = (char)('0' + ((year_2digit / 10U) % 10U));
    out_buf[9]  = (char)('0' + (year_2digit % 10U));
    out_buf[10] = '\0';
}

/* Direct RTC Operations & SystemData Synchronization */
bool AppClock_Init(void)
{
    DS3231_DateTimeType dt;
    bool rtc_ok = BSP_DS3231_Init();

    if (rtc_ok && BSP_DS3231_ReadDateTime(&dt))
    {
        SystemData_UpdateDateTime(&dt);
    }
    else
    {
        SystemData_SetRtcError(true);
        rtc_ok = false;
    }

    s_last_rtc_poll_ms = SCH_SystemTick();
    s_force_rtc_sync   = true;
    s_clock_co_state   = APP_CLOCK_CO_WAIT_INTERVAL;

    return rtc_ok;
}

void AppClock_RequestImmediateSync(void)
{
    s_force_rtc_sync = true;
}

AppClock_Status_t AppClock_ReadTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds)
{
    if (hours == NULL || minutes == NULL || seconds == NULL)
    {
        return APP_CLOCK_ERR_BAD_FORMAT;
    }

    if (!BSP_DS3231_ReadTime(hours, minutes, seconds))
    {
        SystemData_SetRtcError(true);
        return APP_CLOCK_ERR_RTC_FAIL;
    }

    SystemData_UpdateTimeOnly(*hours, *minutes, *seconds);
    return APP_CLOCK_OK;
}

AppClock_Status_t AppClock_ReadDate(uint8_t *date, uint8_t *month, uint8_t *year, uint8_t *day)
{
    if (date == NULL || month == NULL || year == NULL || day == NULL)
    {
        return APP_CLOCK_ERR_BAD_FORMAT;
    }

    if (!BSP_DS3231_ReadDate(date, month, year, day))
    {
        SystemData_SetRtcError(true);
        return APP_CLOCK_ERR_RTC_FAIL;
    }

    SystemData_UpdateDateOnly(*date, *month, *year, *day);
    return APP_CLOCK_OK;
}

AppClock_Status_t AppClock_ReadDateTime(DS3231_DateTimeType *dt)
{
    if (dt == NULL)
    {
        return APP_CLOCK_ERR_BAD_FORMAT;
    }

    if (!BSP_DS3231_ReadDateTime(dt))
    {
        SystemData_SetRtcError(true);
        return APP_CLOCK_ERR_RTC_FAIL;
    }

    SystemData_UpdateDateTime(dt);
    return APP_CLOCK_OK;
}

AppClock_Status_t AppClock_SetTime(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    if (!DS3231_IsValidTime(hours, minutes, seconds))
    {
        return APP_CLOCK_ERR_OUT_OF_RANGE;
    }

    if (!BSP_DS3231_SetTime(hours, minutes, seconds))
    {
        SystemData_SetRtcError(true);
        return APP_CLOCK_ERR_RTC_FAIL;
    }

    SystemData_UpdateTimeOnly(hours, minutes, seconds);
    s_force_rtc_sync = true;
    return APP_CLOCK_OK;
}

AppClock_Status_t AppClock_SetDate(uint8_t date, uint8_t month, uint16_t full_year, uint8_t day)
{
    uint8_t year_2digit;

    if (!DS3231_IsValidDateRange(date, month, full_year, day))
    {
        return APP_CLOCK_ERR_OUT_OF_RANGE;
    }

    if (!DS3231_IsValidCalendarDate(date, month, full_year))
    {
        return APP_CLOCK_ERR_INVALID_DATE;
    }

    year_2digit = (uint8_t)(full_year - 2000U);
    if (!BSP_DS3231_SetDate(date, month, year_2digit, day))
    {
        SystemData_SetRtcError(true);
        return APP_CLOCK_ERR_RTC_FAIL;
    }

    SystemData_UpdateDateOnly(date, month, year_2digit, day);
    s_force_rtc_sync = true;
    return APP_CLOCK_OK;
}

AppClock_Status_t AppClock_SetDateTime(uint8_t date,
                                       uint8_t month,
                                       uint16_t full_year,
                                       uint8_t hours,
                                       uint8_t minutes,
                                       uint8_t seconds,
                                       uint8_t day)
{
    DS3231_DateTimeType dt;

    /* Atomic parameter validation: do not write anything if any field is invalid */
    if (!DS3231_IsValidTime(hours, minutes, seconds) ||
        !DS3231_IsValidDateRange(date, month, full_year, day))
    {
        return APP_CLOCK_ERR_OUT_OF_RANGE;
    }

    if (!DS3231_IsValidCalendarDate(date, month, full_year))
    {
        return APP_CLOCK_ERR_INVALID_DATE;
    }

    dt.date    = date;
    dt.month   = month;
    dt.year    = (uint8_t)(full_year - 2000U);
    dt.day     = day;
    dt.hours   = hours;
    dt.minutes = minutes;
    dt.seconds = seconds;

    if (!BSP_DS3231_SetDateTime(&dt))
    {
        SystemData_SetRtcError(true);
        return APP_CLOCK_ERR_RTC_FAIL;
    }

    SystemData_UpdateDateTime(&dt);
    s_force_rtc_sync = true;
    return APP_CLOCK_OK;
}

AppClock_Status_t AppClock_SetTimeFromStr(const char *time_str)
{
    uint8_t hh, mm, ss;
    AppClock_Status_t status = AppClock_ParseTimeStr(time_str, &hh, &mm, &ss);
    if (status != APP_CLOCK_OK)
    {
        return status;
    }
    return AppClock_SetTime(hh, mm, ss);
}

AppClock_Status_t AppClock_SetDateFromStr(const char *date_str, const char *dow_str)
{
    uint8_t  dd, mm, dow;
    uint16_t yyyy;
    AppClock_Status_t status = AppClock_ParseDateStr(date_str, dow_str, &dd, &mm, &yyyy, &dow);
    if (status != APP_CLOCK_OK)
    {
        return status;
    }
    return AppClock_SetDate(dd, mm, yyyy, dow);
}

AppClock_Status_t AppClock_SetDateTimeFromStr(const char *date_str,
                                              const char *time_str,
                                              const char *dow_str)
{
    uint8_t  dd, mm, dow, hh, min, ss;
    uint16_t yyyy;
    AppClock_Status_t status;

    /* Validate all parameters before touching DS3231 */
    status = AppClock_ParseDateStr(date_str, dow_str, &dd, &mm, &yyyy, &dow);
    if (status != APP_CLOCK_OK)
    {
        return status;
    }

    status = AppClock_ParseTimeStr(time_str, &hh, &min, &ss);
    if (status != APP_CLOCK_OK)
    {
        return status;
    }

    return AppClock_SetDateTime(dd, mm, yyyy, hh, min, ss, dow);
}

/* Co-routine RTOS Task */
void AppClock_Task(void)
{
    uint32_t now_ms = SCH_SystemTick();

    switch (s_clock_co_state)
    {
        case APP_CLOCK_CO_WAIT_INTERVAL:
            if (s_force_rtc_sync ||
                (uint32_t)(now_ms - s_last_rtc_poll_ms) >= (uint32_t)APP_CLOCK_RTC_POLL_MS)
            {
                s_last_rtc_poll_ms = now_ms;
                s_force_rtc_sync   = false;
                s_clock_co_state   = APP_CLOCK_CO_READ_DATETIME;
            }
            return; /* Co-routine yield */

        case APP_CLOCK_CO_READ_DATETIME:
        {
            DS3231_DateTimeType dt;
            if (BSP_DS3231_ReadDateTime(&dt))
            {
                SystemData_UpdateDateTime(&dt);
                s_clock_co_state = APP_CLOCK_CO_CHECK_ALARM;
            }
            else
            {
                SystemData_SetRtcError(true);
                s_clock_co_state = APP_CLOCK_CO_WAIT_INTERVAL;
            }
            return; /* Co-routine yield */
        }

        case APP_CLOCK_CO_CHECK_ALARM:
        default:
        {
            bool alarm_fired = false;
            if (BSP_DS3231_CheckAndClearAlarm1(&alarm_fired) && alarm_fired)
            {
                SystemData_SetAlarmTriggered(true);
            }
            s_clock_co_state = APP_CLOCK_CO_WAIT_INTERVAL;
            return; /* Co-routine yield */
        }
    }
}

