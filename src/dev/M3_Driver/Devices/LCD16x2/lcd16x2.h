#ifndef LCD16X2_H
#define LCD16X2_H

#include <stdbool.h>
#include <stdint.h>
#include <avr/io.h>
#include "../../../M4_Middleware/FIFO/fifo.h"

/**
 * Default hardware pin mapping on ATmega324 
 * PA0 = RS, PA1 = EN, PA4..PA7 = D4..D7 (4-bit mode, RW = GND).
 * Can be overridden prior to inclusion if needed.
 */
#ifndef LCD16X2_RS_DDR
#define LCD16X2_RS_DDR   DDRA
#define LCD16X2_RS_PORT  PORTA
#define LCD16X2_RS_PIN   PA0
#endif

#ifndef LCD16X2_EN_DDR
#define LCD16X2_EN_DDR   DDRA
#define LCD16X2_EN_PORT  PORTA
#define LCD16X2_EN_PIN   PA1
#endif

#ifndef LCD16X2_D4_DDR
#define LCD16X2_D4_DDR   DDRA
#define LCD16X2_D4_PORT  PORTA
#define LCD16X2_D4_PIN   PA4
#endif

#ifndef LCD16X2_D5_DDR
#define LCD16X2_D5_DDR   DDRA
#define LCD16X2_D5_PORT  PORTA
#define LCD16X2_D5_PIN   PA5
#endif

#ifndef LCD16X2_D6_DDR
#define LCD16X2_D6_DDR   DDRA
#define LCD16X2_D6_PORT  PORTA
#define LCD16X2_D6_PIN   PA6
#endif

#ifndef LCD16X2_D7_DDR
#define LCD16X2_D7_DDR   DDRA
#define LCD16X2_D7_PORT  PORTA
#define LCD16X2_D7_PIN   PA7
#endif

/**
 * Display dimensions for standard 16x2 character LCD (HD44780 compatible).
 */
#define LCD16X2_ROWS                  2U
#define LCD16X2_COLS                  16U

/**
 * HD44780 instruction set commands.
 */
#define LCD16X2_CMD_CLEAR_DISPLAY     0x01U
#define LCD16X2_CMD_RETURN_HOME       0x02U
#define LCD16X2_CMD_ENTRY_MODE_SET    0x06U
#define LCD16X2_CMD_DISPLAY_OFF       0x08U
#define LCD16X2_CMD_DISPLAY_ON        0x0CU
#define LCD16X2_CMD_CURSOR_ON         0x0EU
#define LCD16X2_CMD_CURSOR_BLINK      0x0FU
#define LCD16X2_CMD_FUNCTION_4BIT_2L  0x28U
#define LCD16X2_CMD_SET_CGRAM_ADDR    0x40U
#define LCD16X2_CMD_SET_DDRAM_ADDR    0x80U

/**
 * Non-blocking initialization state machine states.
 */
typedef enum {
    LCD16X2_STATE_UNINIT = 0,
    LCD16X2_STATE_WAIT_POWER_ON,
    LCD16X2_STATE_INIT_STEP1_WAIT,
    LCD16X2_STATE_INIT_STEP2_WAIT,
    LCD16X2_STATE_INIT_STEP3_WAIT,
    LCD16X2_STATE_CLEAR_WAIT,
    LCD16X2_STATE_READY
} LCD16x2_State_t;

/**
 * Low-level hardware primitives (4-bit mode).
 * Each byte transfer takes ~42 us.
 */
void LCD16x2_InitPins(void);
void LCD16x2_Write4Bits(uint8_t nibble, bool is_data);
void LCD16x2_SendCommand(uint8_t cmd);
void LCD16x2_WriteChar(char c);
void LCD16x2_SetCursor(uint8_t row, uint8_t col);
void LCD16x2_WriteString(const char *str);

/**
 * Blocking initialization (only use before starting scheduler if needed).
 */
void LCD16x2_InitBlocking(void);

/**
 * Initialization and FIFO-backed differential display manager.
 * Uses FIFO_t from M4_Middleware to queue pending LCD operations and avoids _delay_ms.
 */
void            LCD16x2_InitAsync(uint32_t current_tick_ms);
bool            LCD16x2_IsReady(void);
LCD16x2_State_t LCD16x2_GetState(void);

/**
 * Enqueue a raw command or data byte into the LCD middleware FIFO for non-blocking execution.
 */
bool LCD16x2_EnqueueCommand(uint8_t cmd);
bool LCD16x2_EnqueueChar(char c);
uint16_t LCD16x2_PendingQueueCount(void);

/**
 * Step the LCD co-routine state machine:
 * - During initialization: advances 4-bit HD44780 init steps using current_tick_ms.
 * - When ready: scans differential framebuffer, pushes changed characters
 *   into the internal FIFO_t, and drains up to max_ops_per_step items to the LCD hardware.
 * Returns true if display is completely synchronized and FIFO is empty.
 */
bool LCD16x2_TaskStep(uint32_t current_tick_ms, uint8_t max_ops_per_step);

/**
 * Framebuffer manipulation APIs (instant RAM operations, 0 us hardware wait).
 * Only modified character cells are sent to the LCD (Section 3.1).
 */
void LCD16x2_BufferClear(void);
void LCD16x2_InvalidateShadow(void);
void LCD16x2_BufferPutChar(uint8_t row, uint8_t col, char c);
void LCD16x2_BufferPutString(uint8_t row, uint8_t col, const char *str);
void LCD16x2_BufferWriteLinePadded(uint8_t row, const char *str);

/**
 * Format the BTL main clock screen into the desired framebuffer:
 *   Row 0: "dd/mm/yyyy  DOW " (e.g. "26/08/2026  WED ")
 *   Row 1: "hh:mm:ss   L:xxx" (e.g. "14:30:05   L:BLK") or "RTC ERROR " on I2C fault.
 */
void LCD16x2_FormatClockScreen(uint8_t date,
                               uint8_t month,
                               uint8_t year_2digit,
                               const char *dow_str,
                               uint8_t hours,
                               uint8_t minutes,
                               uint8_t seconds,
                               const char *led_mode_tag,
                               bool rtc_error);

#endif /* LCD16X2_H */

