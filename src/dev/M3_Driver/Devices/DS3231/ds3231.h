#ifndef DS3231_H
#define DS3231_H

#include <stdbool.h>
#include <stdint.h>

/**
 * 7-bit I2C slave address for the DS3231 RTC (0xD0 write / 0xD1 read).
 */
#define DS3231_I2C_ADDR            0x68U

/**
 * Register definitions for the DS3231 RTC device.
 */
#define DS3231_REG_SECONDS         0x00U
#define DS3231_REG_MINUTES         0x01U
#define DS3231_REG_HOURS           0x02U
#define DS3231_REG_DAY             0x03U
#define DS3231_REG_DATE            0x04U
#define DS3231_REG_MONTH           0x05U
#define DS3231_REG_YEAR            0x06U
#define DS3231_REG_ALARM1_SECONDS  0x07U
#define DS3231_REG_ALARM1_MINUTES  0x08U
#define DS3231_REG_ALARM1_HOURS    0x09U
#define DS3231_REG_ALARM1_DAY_DATE 0x0AU
#define DS3231_REG_ALARM2_MINUTES  0x0BU
#define DS3231_REG_ALARM2_HOURS    0x0CU
#define DS3231_REG_ALARM2_DAY_DATE 0x0DU
#define DS3231_REG_CONTROL         0x0EU
#define DS3231_REG_STATUS          0x0FU
#define DS3231_REG_AGING_OFFSET    0x10U
#define DS3231_REG_TEMP_MSB        0x11U
#define DS3231_REG_TEMP_LSB        0x12U

/**
 * Control (0x0E) & Status (0x0F) register bit masks.
 */
#define DS3231_CTRL_EOSC           (1U << 7) /* Enable Oscillator (0 = run on VBAT) */
#define DS3231_CTRL_BBSQW          (1U << 6)
#define DS3231_CTRL_CONV           (1U << 5)
#define DS3231_CTRL_INTCN          (1U << 2) /* Interrupt Control (1 = INT/SQW outputs alarm) */
#define DS3231_CTRL_A2IE           (1U << 1)
#define DS3231_CTRL_A1IE           (1U << 0)

#define DS3231_STAT_OSF            (1U << 7) /* Oscillator Stop Flag */
#define DS3231_STAT_EN32KHZ        (1U << 3)
#define DS3231_STAT_BSY            (1U << 2)
#define DS3231_STAT_A2F            (1U << 1)
#define DS3231_STAT_A1F            (1U << 0)

/**
 * Day-of-week convention (1 = Sunday .. 7 = Saturday).
 */
typedef enum {
    DS3231_DOW_INVALID = 0U,
    DS3231_DOW_SUN     = 1U,
    DS3231_DOW_MON     = 2U,
    DS3231_DOW_TUE     = 3U,
    DS3231_DOW_WED     = 4U,
    DS3231_DOW_THU     = 5U,
    DS3231_DOW_FRI     = 6U,
    DS3231_DOW_SAT     = 7U
} DS3231_DayOfWeek_t;

/**
 * Alarm 1 match modes (bits A1M4..A1M1).
 */
typedef enum {
    DS3231_ALARM1_EVERY_SECOND          = 0x0FU,
    DS3231_ALARM1_MATCH_SECONDS         = 0x0EU,
    DS3231_ALARM1_MATCH_MIN_SEC         = 0x0CU,
    DS3231_ALARM1_MATCH_HR_MIN_SEC      = 0x08U,
    DS3231_ALARM1_MATCH_DATE_HR_MIN_SEC = 0x00U,
    DS3231_ALARM1_MATCH_DAY_HR_MIN_SEC  = 0x10U /* Bit 4 sets DY/DT = 1 */
} DS3231_Alarm1Mode_t;

/**
 * Types declarations for the DS3231 RTC device.
 */
typedef struct {
    uint8_t seconds;        /* 0-59 */
    uint8_t minutes;        /* 0-59 */
    uint8_t hours;          /* 0-23, using 24h mode */
    uint8_t day;            /* 1-7 (1=SUN .. 7=SAT) */
    uint8_t date;           /* 1-31 */
    uint8_t month;          /* 1-12 */
    uint8_t year;           /* 0-99 (2000-2099) */
} DS3231_DateTimeType;

/**
 * Types declarations for the DS3231 RTC alarm configuration.
 */
typedef struct {
    uint8_t seconds;        /* 0-59 */
    uint8_t minutes;        /* 0-59 */
    uint8_t hours;          /* 0-23, using 24h mode */
    uint8_t day_date;       /* 1-31 or 1-7 depending on DY/DT bit */
    uint8_t alarm_type;     /* Alarm type (A1M1, A1M2, A1M3, A1M4) */
} DS3231_AlarmType;

/**
 * Temperature reading from DS3231 internal sensor (0.25 C resolution).
 */
typedef struct {
    int8_t  integer;          /* Signed integer part in Celsius (e.g., 28 or -5) */
    uint8_t frac_hundredths;  /* Fractional part in hundredths: 0, 25, 50, or 75 */
    bool    negative;         /* True when temperature is below 0.00 C */
} DS3231_TempType;

/**
 * Function prototypes
 */

/* Initialization and connectivity */
bool DS3231_Init(void);
bool DS3231_IsConnected(void);
bool DS3231_HasBusFault(void);

/* Atomic burst read/write of date and time registers */
bool DS3231_ReadDateTime(DS3231_DateTimeType *dt);
bool DS3231_SetDateTime(const DS3231_DateTimeType *dt);

bool DS3231_ReadTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds);
bool DS3231_SetTime(uint8_t hours, uint8_t minutes, uint8_t seconds);

bool DS3231_ReadDate(uint8_t *date, uint8_t *month, uint8_t *year, uint8_t *day);
bool DS3231_SetDate(uint8_t date, uint8_t month, uint8_t year, uint8_t day);

/* Internal temperature sensor (registers 0x11 - 0x12) */
bool DS3231_ReadTemperature(DS3231_TempType *temp);

/* Alarm 1 support */
bool DS3231_SetAlarm1(const DS3231_AlarmType *alarm, bool enable_interrupt);
bool DS3231_DisableAlarm1(void);
bool DS3231_CheckAndClearAlarm1(bool *triggered);

/* BCD conversion & calendar validation helpers */
uint8_t     DS3231_BcdToDec(uint8_t bcd);
uint8_t     DS3231_DecToBcd(uint8_t dec);
bool        DS3231_IsLeapYear(uint16_t full_year);
uint8_t     DS3231_DaysInMonth(uint8_t month, uint16_t full_year);
bool        DS3231_IsValidTime(uint8_t hours, uint8_t minutes, uint8_t seconds);
bool        DS3231_IsValidDateRange(uint8_t date, uint8_t month, uint16_t full_year, uint8_t day);
bool        DS3231_IsValidCalendarDate(uint8_t date, uint8_t month, uint16_t full_year);
const char *DS3231_DayToString(uint8_t day);
uint8_t     DS3231_StringToDay(const char *str);

#endif /* DS3231_H */
