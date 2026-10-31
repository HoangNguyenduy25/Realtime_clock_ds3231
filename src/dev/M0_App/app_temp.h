#ifndef APP_TEMP_H_
#define APP_TEMP_H_

#include <stdbool.h>
#include <stdint.h>

#include "../M2_BSP/DS3231/bsp_ds3231.h"

/**
 * Default background polling period for the DS3231 internal temperature sensor (ms).
 */
#define APP_TEMP_POLL_PERIOD_MS 1000U

/**
 * Status codes for temperature operations (maps to Section 4.4 & 5.2.13).
 */
typedef enum
{
    APP_TEMP_OK = 0,
    APP_TEMP_ERR_RTC_FAIL /* E05: RTC ERROR (DS3231 NACK or bus fault) */
} AppTemp_Status_t;

/**
 * Initialize temperature application state and perform an initial sensor read.
 */
bool AppTemp_Init(void);

/**
 * Cooperative RTOS Co-routine task for periodic DS3231 temperature acquisition (FR-10).
 * Register in the scheduler task table (e.g. period = 100..500 ms).
 */
void AppTemp_Task(void);

/**
 * Read temperature directly from DS3231 registers 0x11-0x12 and update SystemData (Section 5.2.13).
 */
AppTemp_Status_t AppTemp_ReadNow(DS3231_TempType *temp);

/**
 * Retrieve the latest temperature for TEMP? command.
 */
AppTemp_Status_t AppTemp_GetTemperature(DS3231_TempType *temp);

/**
 * Format a DS3231_TempType reading into a two-decimal ASCII string (e.g. "28.25" or "-5.50")
 * suitable for "+OK,TEMP,<val>,C" responses without requiring floating-point printf.
 */
void AppTemp_FormatString(const DS3231_TempType *temp, char *out_buf, uint8_t buf_size);

#endif /* APP_TEMP_H_ */

