#ifndef APP_LED_H_
#define APP_LED_H_

#include <stdbool.h>
#include <stdint.h>

#include "../M1_Sys/SystemData/system_data.h"

/**
 * Status codes for LED operations (maps to protocol error codes in Section 4.4 & 5.2.4-5.2.6).
 */
typedef enum
{
    APP_LED_OK = 0,
    APP_LED_ERR_OUT_OF_RANGE, /* E03: PARAM OUT OF RANGE (period outside 10..5000 ms, or invalid keyword) */
    APP_LED_ERR_BAD_FORMAT    /* E04: BAD FORMAT (non-integer blink period) */
} AppLed_Status_t;

/**
 * Initialize the LED hardware (PB0 active high, initially OFF) and reset the LED co-routine.
 */
bool AppLed_Init(void);

/**
 * Cooperative RTOS Co-routine task for LED control (FR-04, NFR-01).
 * Register in the scheduler task table (e.g. period = 1..5 ms).
 * Uses SCH_TIM_LED to toggle the LED in BLINK mode (T/2 ms ON, T/2 ms OFF) without blocking delays.
 */
void AppLed_Task(void);

/**
 * Command-facing APIs to control LED state and update SystemData (FR-04, FR-08).
 */
void            AppLed_SetOn(void);
void            AppLed_SetOff(void);
AppLed_Status_t AppLed_SetBlink(uint16_t period_ms);
AppLed_Status_t AppLed_SetBlinkFromStr(const char *period_str, uint16_t *out_period_ms);

/**
 * Query current LED operating mode and blink period for LED? command (Section 5.2.6).
 */
SystemData_LedMode AppLed_GetMode(void);
uint16_t           AppLed_GetBlinkPeriod(void);
const char        *AppLed_GetLcdTag(void);

#endif /* APP_LED_H_ */

