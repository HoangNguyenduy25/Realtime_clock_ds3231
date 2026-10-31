#ifndef BSP_LCD_H_
#define BSP_LCD_H_

#include <stdbool.h>
#include <stdint.h>

#include "../board.h"
#include "../../M3_Driver/Devices/LCD16x2/lcd16x2.h"
#include "../../M3_Driver/Devices/DS3231/ds3231.h"

/**
 * Maximum number of queued LCD operations (command/character bytes) executed
 * per BSP_LCD_TaskStep() call (~42 us per operation -> 4 ops ~= 170 us << 5 ms NFR-01).
 */
#define BSP_LCD_OPS_PER_STEP 4U

/**
 * Initialize board GPIO pins (PA0=RS, PA1=EN, PA4..PA7=D4..D7) via M3 DIO driver
 * and start the non-blocking 4-bit LCD initialization state machine.
 */
bool BSP_LCD_Init(uint32_t current_tick_ms);

/**
 * Blocking initialization (use only during pre-scheduler startup if desired).
 */
bool BSP_LCD_InitBlocking(void);

/**
 * True when the non-blocking 4-bit HD44780 initialization sequence has completed.
 */
bool BSP_LCD_IsReady(void);

/**
 * Non-blocking Co-routine step for the LCD.
 * Advances initialization or drains modified characters from the middleware FIFO to the LCD.
 * Returns true when the LCD hardware matches the desired screen buffer completely.
 */
bool BSP_LCD_TaskStep(uint32_t current_tick_ms);

/**
 * Framebuffer update functions (non-blocking, 0 us hardware wait; only changed characters
 * are transmitted on subsequent BSP_LCD_TaskStep() calls).
 */
void BSP_LCD_Clear(void);
void BSP_LCD_ForceRedraw(void);
void BSP_LCD_PutChar(uint8_t row, uint8_t col, char c);
void BSP_LCD_PutString(uint8_t row, uint8_t col, const char *str);
void BSP_LCD_WriteLine(uint8_t row, const char *str);

/**
 * Format the standard BTL main screen (Section 3.1) into the differential framebuffer:
 *   Row 0: "dd/mm/yyyy  DOW " (e.g. "26/08/2026  WED ")
 *   Row 1: "hh:mm:ss   L:xxx" (e.g. "14:30:05   L:BLK") or "RTC ERROR       " when rtc_error is true.
 */
void BSP_LCD_UpdateClockView(const DS3231_DateTimeType *dt,
                             const char *led_mode_tag,
                             bool rtc_error);

#endif /* BSP_LCD_H_ */

