#ifndef APP_CLOCK_H_
#define APP_CLOCK_H_

#include <stdbool.h>
#include <stdint.h>

#include "../M2_BSP/DS3231/bsp_ds3231.h"

/**
 * Maximum DS3231 polling period required by FR-01 is 500 ms.
 * Polling every 250 ms guarantees smooth 1-second time updates without beat drift.
 */
#define APP_CLOCK_RTC_POLL_MS 250U

/**
 * Status codes for Clock / RTC operations (maps directly to Section 4.4 error codes).
 */
typedef enum
{
    APP_CLOCK_OK = 0,
    APP_CLOCK_ERR_OUT_OF_RANGE, /* E03: PARAM OUT OF RANGE */
    APP_CLOCK_ERR_BAD_FORMAT,   /* E04: BAD FORMAT */
    APP_CLOCK_ERR_RTC_FAIL,     /* E05: RTC ERROR */
    APP_CLOCK_ERR_INVALID_DATE  /* E07: INVALID DATE (e.g. 31/02/2026 or non-leap 29/02) */
} AppClock_Status_t;

/**
 * Initialize the DS3231 RTC peripheral and populate initial SystemData date/time.
 */
bool AppClock_Init(void);

/**
 * Cooperative RTOS Co-routine task for periodic DS3231 reading (every <= 250 ms)
 * and SystemData synchronization (FR-01, FR-09, NFR-01).
 * Register in the scheduler task table (e.g. period = 50..250 ms).
 */
void AppClock_Task(void);

/**
 * Request an immediate RTC read on the next AppClock_Task() execution.
 */
void AppClock_RequestImmediateSync(void);

/**
 * Read current Time, Date, or combined DateTime from DS3231 and update SystemData (FR-05).
 */
AppClock_Status_t AppClock_ReadTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds);
AppClock_Status_t AppClock_ReadDate(uint8_t *date, uint8_t *month, uint8_t *year, uint8_t *day);
AppClock_Status_t AppClock_ReadDateTime(DS3231_DateTimeType *dt);

/**
 * Validate and write new Time, Date, or combined DateTime to DS3231 and SystemData (FR-06, FR-07).
 */
AppClock_Status_t AppClock_SetTime(uint8_t hours, uint8_t minutes, uint8_t seconds);
AppClock_Status_t AppClock_SetDate(uint8_t date, uint8_t month, uint16_t full_year, uint8_t day);
AppClock_Status_t AppClock_SetDateTime(uint8_t date,
                                       uint8_t month,
                                       uint16_t full_year,
                                       uint8_t hours,
                                       uint8_t minutes,
                                       uint8_t seconds,
                                       uint8_t day);

/**
 * Parse and validate protocol strings ("hh:mm:ss", "dd/mm/yyyy", "DOW") per Section 5.2.10-5.2.12.
 * Distinguishes E04 (BAD FORMAT), E03 (PARAM OUT OF RANGE), and E07 (INVALID DATE).
 */
AppClock_Status_t AppClock_ParseTimeStr(const char *time_str,
                                        uint8_t *hours,
                                        uint8_t *minutes,
                                        uint8_t *seconds);

AppClock_Status_t AppClock_ParseDateStr(const char *date_str,
                                        const char *dow_str,
                                        uint8_t *date,
                                        uint8_t *month,
                                        uint16_t *full_year,
                                        uint8_t *day);

/**
 * High-level string-based setters for TIME, DATE, and DT commands (atomic validation before write).
 */
AppClock_Status_t AppClock_SetTimeFromStr(const char *time_str);
AppClock_Status_t AppClock_SetDateFromStr(const char *date_str, const char *dow_str);
AppClock_Status_t AppClock_SetDateTimeFromStr(const char *date_str,
                                              const char *time_str,
                                              const char *dow_str);

/**
 * Formatting helpers for UART responses ("hh:mm:ss" and "dd/mm/yyyy").
 */
void AppClock_FormatTimeStr(uint8_t hours, uint8_t minutes, uint8_t seconds, char *out_buf);
void AppClock_FormatDateStr(uint8_t date, uint8_t month, uint8_t year_2digit, char *out_buf);

#endif /* APP_CLOCK_H_ */

