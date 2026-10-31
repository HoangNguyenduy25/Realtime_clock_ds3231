#ifndef APP_LCD_H_
#define APP_LCD_H_

#include <stdbool.h>
#include <stdint.h>

/**
 * Maximum periodic refresh check interval for the LCD (ms) in case a full sync is needed.
 */
#define APP_LCD_REFRESH_PERIOD_MS 250U

/**
 * Initialize the LCD 16x2 hardware pins and start the non-blocking 4-bit initialization co-routine.
 */
bool AppLcd_Init(void);

/**
 * Cooperative RTOS Co-routine task for LCD 16x2 display management.
 * Register in the scheduler task table.
 *
 * Co-routine stages:
 *   1. Non-blocking 4-bit HD44780 power-on initialization.
 *   2. Observes SystemData (DateTime, LED mode tag ON/OFF/BLK, RTC error flag) when dirty.
 *   3. Formats the 2x16 target screen buffer:
 *        Row 0: "26/08/2026  WED "
 *        Row 1: "14:30:05   L:BLK" (or "RTC ERROR       " when DS3231 is faulted)
 *   4.Incrementally flushes only modified characters from the middleware FIFO to the LCD.
 */
void AppLcd_Task(void);

/**
 * Force a complete redraw of all 32 character cells on the LCD without clearing DDRAM (avoids flicker).
 */
void AppLcd_RequestRedraw(void);

/**
 * Returns true once the non-blocking 4-bit LCD initialization has completed.
 */
bool AppLcd_IsReady(void);

#endif /* APP_LCD_H_ */

