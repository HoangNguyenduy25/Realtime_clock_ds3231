#ifndef SYSTEM_DATA_H_
#define SYSTEM_DATA_H_

#include <stdbool.h>
#include <stdint.h>

#include "../../M2_BSP/DS3231/bsp_ds3231.h"

/**
 * LED blink period bounds specified(10 ms .. 5000 ms).
 */
#define SYSTEM_DATA_BLINK_MIN_MS     10U
#define SYSTEM_DATA_BLINK_MAX_MS     5000U
#define SYSTEM_DATA_BLINK_DEFAULT_MS 1000U

/**
 * Heartbeat timing record.
 */
typedef struct
{
    uint32_t on_ms;
    uint32_t off_ms;
} SystemData_HeartbeatTiming;

/**
 * Heartbeat status record
 */
typedef struct
{
    bool enabled;
    bool led_on;
} SystemData_HeartbeatStatus;

/**
 * High-level LED operating mode for protocol commands (LED,ON / LED,OFF / LED,BLINK,<T>)
 * and LCD status indicator (L:ON / L:OFF / L:BLK).
 */
typedef enum
{
    SYS_LED_MODE_OFF = 0,
    SYS_LED_MODE_ON,
    SYS_LED_MODE_BLINK
} SystemData_LedMode;

typedef struct
{
    SystemData_LedMode mode;            /* Current LED mode: OFF, ON, or BLINK */
    uint16_t           blink_period_ms; /* Full blink period T in ms (10..5000) */
    bool               pin_state;       /* Current physical output level on LED pin */
} SystemData_LedRecord;

/**
 * Real-Time Clock (DS3231) shared state record (single main-loop context only).
 */
typedef struct
{
    DS3231_DateTimeType datetime;          /* Cached date & time (dd/mm/yy, hh:mm:ss, dow) */
    DS3231_TempType     temperature;       /* Cached DS3231 temperature (0.25 C resolution) */
    bool                time_valid;        /* True after at least one successful RTC read/write */
    bool                temp_valid;        /* True after at least one successful temperature read */
    bool                rtc_error;         /* True when DS3231 is disconnected / I2C NACK */
    bool                alarm_triggered;   /* True when DS3231 Alarm 1 flag was asserted */
    bool                display_dirty;     /* True when RTC or LED state changed and LCD needs update */
} SystemData_RtcRecord;


/* Initialization */

/**
 * Initialize all SystemData records to deterministic defaults.
 * Must be called from main-loop context prior to starting scheduled tasks.
 */
void SystemData_Init(void);

/* Heartbeat & LED State Accessors */
void SystemData_GetHeartbeatTiming(SystemData_HeartbeatTiming *timing);
bool SystemData_SetHeartbeatTiming(const SystemData_HeartbeatTiming *timing);

void SystemData_GetHeartbeatStatus(SystemData_HeartbeatStatus *status);
void SystemData_SetHeartbeatStatus(const SystemData_HeartbeatStatus *status);

void SystemData_GetLedRecord(SystemData_LedRecord *record);
SystemData_LedMode SystemData_GetLedMode(void);
uint16_t SystemData_GetLedBlinkPeriod(void);

void SystemData_SetLedModeOn(void);
void SystemData_SetLedModeOff(void);
bool SystemData_SetLedModeBlink(uint16_t period_ms);
void SystemData_SetLedPinState(bool pin_on);

/**
 * Returns the 3-character LCD status tag for the current LED mode:
 * "ON", "OFF", or "BLK".
 */
const char *SystemData_GetLedLcdTag(void);


/* RTC (DS3231) State Accessors */
void SystemData_GetRtcRecord(SystemData_RtcRecord *record);

bool SystemData_GetDateTime(DS3231_DateTimeType *dt);
void SystemData_UpdateDateTime(const DS3231_DateTimeType *dt);
void SystemData_UpdateTimeOnly(uint8_t hours, uint8_t minutes, uint8_t seconds);
void SystemData_UpdateDateOnly(uint8_t date, uint8_t month, uint8_t year, uint8_t day);

bool SystemData_GetTemperature(DS3231_TempType *temp);
void SystemData_UpdateTemperature(const DS3231_TempType *temp);

bool SystemData_HasRtcError(void);
void SystemData_SetRtcError(bool error);

bool SystemData_IsAlarmTriggered(void);
void SystemData_SetAlarmTriggered(bool triggered);

/* Display Dirty Flag Coordination */
bool SystemData_IsDisplayDirty(void);
void SystemData_ClearDisplayDirty(void);
void SystemData_MarkDisplayDirty(void);

#endif /* SYSTEM_DATA_H_ */
