#include "system_data.h"

#include <stddef.h>
#include <string.h>

/**
 * Internal single-context data store.
 * Accessed exclusively by cooperative tasks/co-routines in the main loop;
 * ISRs must never read or modify these records.
 */
typedef struct
{
    SystemData_HeartbeatTiming heartbeat_timing;
    SystemData_HeartbeatStatus heartbeat_status;
    SystemData_LedRecord       led;
    SystemData_RtcRecord       rtc;
} SystemData_Context;

static SystemData_Context s_sys_data;

void SystemData_Init(void)
{
    memset(&s_sys_data, 0, sizeof(s_sys_data));

    /* Default LED state: OFF, default blink period 1000 ms (500 ms ON / 500 ms OFF) */
    s_sys_data.heartbeat_timing.on_ms  = SYSTEM_DATA_BLINK_DEFAULT_MS / 2U;
    s_sys_data.heartbeat_timing.off_ms = SYSTEM_DATA_BLINK_DEFAULT_MS / 2U;

    s_sys_data.heartbeat_status.enabled = false;
    s_sys_data.heartbeat_status.led_on  = false;

    s_sys_data.led.mode            = SYS_LED_MODE_OFF;
    s_sys_data.led.blink_period_ms = SYSTEM_DATA_BLINK_DEFAULT_MS;
    s_sys_data.led.pin_state       = false;

    /* Default RTC state before first DS3231 read */
    s_sys_data.rtc.datetime.date    = 1U;
    s_sys_data.rtc.datetime.month   = 1U;
    s_sys_data.rtc.datetime.year    = 26U;
    s_sys_data.rtc.datetime.day     = DS3231_DOW_THU;
    s_sys_data.rtc.datetime.hours   = 0U;
    s_sys_data.rtc.datetime.minutes = 0U;
    s_sys_data.rtc.datetime.seconds = 0U;

    s_sys_data.rtc.temperature.integer         = 25;
    s_sys_data.rtc.temperature.frac_hundredths = 0U;
    s_sys_data.rtc.temperature.negative        = false;

    s_sys_data.rtc.time_valid      = false;
    s_sys_data.rtc.temp_valid      = false;
    s_sys_data.rtc.rtc_error       = false;
    s_sys_data.rtc.alarm_triggered = false;
    s_sys_data.rtc.display_dirty   = true;
}

/* Heartbeat & LED State Accessors (Single-Context, Non-ISR) */
void SystemData_GetHeartbeatTiming(SystemData_HeartbeatTiming *timing)
{
    if (timing != NULL)
    {
        *timing = s_sys_data.heartbeat_timing;
    }
}

bool SystemData_SetHeartbeatTiming(const SystemData_HeartbeatTiming *timing)
{
    uint32_t total_ms;

    if (timing == NULL || timing->on_ms == 0UL || timing->off_ms == 0UL)
    {
        return false;
    }

    total_ms = timing->on_ms + timing->off_ms;
    if (total_ms < SYSTEM_DATA_BLINK_MIN_MS || total_ms > SYSTEM_DATA_BLINK_MAX_MS)
    {
        return false;
    }

    s_sys_data.heartbeat_timing    = *timing;
    s_sys_data.led.blink_period_ms = (uint16_t)total_ms;
    s_sys_data.rtc.display_dirty   = true;
    return true;
}

void SystemData_GetHeartbeatStatus(SystemData_HeartbeatStatus *status)
{
    if (status != NULL)
    {
        *status = s_sys_data.heartbeat_status;
    }
}

void SystemData_SetHeartbeatStatus(const SystemData_HeartbeatStatus *status)
{
    if (status == NULL)
    {
        return;
    }

    s_sys_data.heartbeat_status = *status;
    s_sys_data.led.pin_state    = status->led_on;
    if (status->enabled)
    {
        s_sys_data.led.mode = SYS_LED_MODE_BLINK;
    }
    else
    {
        s_sys_data.led.mode = status->led_on ? SYS_LED_MODE_ON : SYS_LED_MODE_OFF;
    }
    s_sys_data.rtc.display_dirty = true;
}

void SystemData_GetLedRecord(SystemData_LedRecord *record)
{
    if (record != NULL)
    {
        *record = s_sys_data.led;
    }
}

SystemData_LedMode SystemData_GetLedMode(void)
{
    return s_sys_data.led.mode;
}

uint16_t SystemData_GetLedBlinkPeriod(void)
{
    return s_sys_data.led.blink_period_ms;
}

void SystemData_SetLedModeOn(void)
{
    if (s_sys_data.led.mode != SYS_LED_MODE_ON || !s_sys_data.led.pin_state)
    {
        s_sys_data.rtc.display_dirty = true;
    }
    s_sys_data.led.mode                 = SYS_LED_MODE_ON;
    s_sys_data.led.pin_state            = true;
    s_sys_data.heartbeat_status.enabled = false;
    s_sys_data.heartbeat_status.led_on  = true;
}

void SystemData_SetLedModeOff(void)
{
    if (s_sys_data.led.mode != SYS_LED_MODE_OFF || s_sys_data.led.pin_state)
    {
        s_sys_data.rtc.display_dirty = true;
    }
    s_sys_data.led.mode                 = SYS_LED_MODE_OFF;
    s_sys_data.led.pin_state            = false;
    s_sys_data.heartbeat_status.enabled = false;
    s_sys_data.heartbeat_status.led_on  = false;
}

bool SystemData_SetLedModeBlink(uint16_t period_ms)
{
    uint16_t half_ms;

    if (period_ms < SYSTEM_DATA_BLINK_MIN_MS || period_ms > SYSTEM_DATA_BLINK_MAX_MS)
    {
        return false;
    }

    half_ms = period_ms / 2U;
    s_sys_data.led.mode                = SYS_LED_MODE_BLINK;
    s_sys_data.led.blink_period_ms     = period_ms;
    s_sys_data.heartbeat_timing.on_ms  = half_ms;
    s_sys_data.heartbeat_timing.off_ms = period_ms - half_ms;
    s_sys_data.heartbeat_status.enabled = true;
    s_sys_data.rtc.display_dirty       = true;
    return true;
}

void SystemData_SetLedPinState(bool pin_on)
{
    s_sys_data.led.pin_state           = pin_on;
    s_sys_data.heartbeat_status.led_on = pin_on;
}

const char *SystemData_GetLedLcdTag(void)
{
    switch (s_sys_data.led.mode)
    {
        case SYS_LED_MODE_ON:
            return "ON";
        case SYS_LED_MODE_BLINK:
            return "BLK";
        case SYS_LED_MODE_OFF:
        default:
            return "OFF";
    }
}

/* RTC (DS3231) State Accessors (Single-Context, Non-ISR) */
void SystemData_GetRtcRecord(SystemData_RtcRecord *record)
{
    if (record != NULL)
    {
        *record = s_sys_data.rtc;
    }
}

bool SystemData_GetDateTime(DS3231_DateTimeType *dt)
{
    if (dt != NULL)
    {
        *dt = s_sys_data.rtc.datetime;
    }
    return s_sys_data.rtc.time_valid && !s_sys_data.rtc.rtc_error;
}

void SystemData_UpdateDateTime(const DS3231_DateTimeType *dt)
{
    if (dt == NULL)
    {
        return;
    }

    if (memcmp(&s_sys_data.rtc.datetime, dt, sizeof(DS3231_DateTimeType)) != 0 ||
        !s_sys_data.rtc.time_valid ||
        s_sys_data.rtc.rtc_error)
    {
        s_sys_data.rtc.datetime      = *dt;
        s_sys_data.rtc.display_dirty = true;
    }
    s_sys_data.rtc.time_valid = true;
    s_sys_data.rtc.rtc_error  = false;
}

void SystemData_UpdateTimeOnly(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    if (s_sys_data.rtc.datetime.hours != hours ||
        s_sys_data.rtc.datetime.minutes != minutes ||
        s_sys_data.rtc.datetime.seconds != seconds ||
        s_sys_data.rtc.rtc_error)
    {
        s_sys_data.rtc.datetime.hours   = hours;
        s_sys_data.rtc.datetime.minutes = minutes;
        s_sys_data.rtc.datetime.seconds = seconds;
        s_sys_data.rtc.display_dirty    = true;
    }
    s_sys_data.rtc.time_valid = true;
    s_sys_data.rtc.rtc_error  = false;
}

void SystemData_UpdateDateOnly(uint8_t date, uint8_t month, uint8_t year, uint8_t day)
{
    if (s_sys_data.rtc.datetime.date != date ||
        s_sys_data.rtc.datetime.month != month ||
        s_sys_data.rtc.datetime.year != year ||
        s_sys_data.rtc.datetime.day != day ||
        s_sys_data.rtc.rtc_error)
    {
        s_sys_data.rtc.datetime.date  = date;
        s_sys_data.rtc.datetime.month = month;
        s_sys_data.rtc.datetime.year  = year;
        s_sys_data.rtc.datetime.day   = day;
        s_sys_data.rtc.display_dirty  = true;
    }
    s_sys_data.rtc.time_valid = true;
    s_sys_data.rtc.rtc_error  = false;
}

bool SystemData_GetTemperature(DS3231_TempType *temp)
{
    if (temp != NULL)
    {
        *temp = s_sys_data.rtc.temperature;
    }
    return s_sys_data.rtc.temp_valid && !s_sys_data.rtc.rtc_error;
}

void SystemData_UpdateTemperature(const DS3231_TempType *temp)
{
    if (temp == NULL)
    {
        return;
    }
    s_sys_data.rtc.temperature = *temp;
    s_sys_data.rtc.temp_valid  = true;
}

bool SystemData_HasRtcError(void)
{
    return s_sys_data.rtc.rtc_error;
}

void SystemData_SetRtcError(bool error)
{
    if (s_sys_data.rtc.rtc_error != error)
    {
        s_sys_data.rtc.rtc_error     = error;
        s_sys_data.rtc.display_dirty = true;
    }
}

bool SystemData_IsAlarmTriggered(void)
{
    return s_sys_data.rtc.alarm_triggered;
}

void SystemData_SetAlarmTriggered(bool triggered)
{
    s_sys_data.rtc.alarm_triggered = triggered;
}

/* Display Dirty Flag Coordination */
bool SystemData_IsDisplayDirty(void)
{
    return s_sys_data.rtc.display_dirty;
}

void SystemData_ClearDisplayDirty(void)
{
    s_sys_data.rtc.display_dirty = false;
}

void SystemData_MarkDisplayDirty(void)
{
    s_sys_data.rtc.display_dirty = true;
}

