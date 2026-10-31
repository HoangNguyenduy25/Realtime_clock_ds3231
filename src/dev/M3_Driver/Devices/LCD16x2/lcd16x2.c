#include "lcd16x2.h"

#ifndef F_CPU
#define F_CPU 8000000UL
#endif

#include <stddef.h>
#include <util/atomic.h>
#include <util/delay.h>

/**
 * Internal FIFO buffer size (must be even since each operation pushes 2 bytes: [type, value]).
 */
#define LCD16X2_FIFO_CAPACITY  128U
#define LCD16X2_OP_CMD         0x00U
#define LCD16X2_OP_DATA        0x01U

/* DDRAM row base addresses for 16x2 HD44780 display. */
static const uint8_t k_row_offsets[LCD16X2_ROWS] = { 0x00U, 0x40U };

/* Middleware FIFO instance for non-blocking command/character streaming. */
static FIFO_t  s_lcd_fifo;
static uint8_t s_lcd_fifo_storage[LCD16X2_FIFO_CAPACITY];

/* Desired screen content vs. shadow buffer for differential updates */
static char s_target_buf[LCD16X2_ROWS][LCD16X2_COLS];
static char s_shadow_buf[LCD16X2_ROWS][LCD16X2_COLS];

/* Co-routine state machine context for non-blocking init and rendering. */
static LCD16x2_State_t s_lcd_state = LCD16X2_STATE_UNINIT;
static uint32_t        s_wait_start_ms = 0U;
static uint8_t         s_queued_row = 0xFFU;
static uint8_t         s_queued_col = 0xFFU;

/* Low-Level AVR GPIO & Bus Primitives */
static void lcd16x2_pulse_enable(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        LCD16X2_EN_PORT |= _BV(LCD16X2_EN_PIN);
    }
    _delay_us(1);
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        LCD16X2_EN_PORT &= (uint8_t)~_BV(LCD16X2_EN_PIN);
    }
}

static void lcd16x2_set_data_nibble(uint8_t nibble)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if ((nibble & 0x01U) != 0U) LCD16X2_D4_PORT |= _BV(LCD16X2_D4_PIN);
        else                        LCD16X2_D4_PORT &= (uint8_t)~_BV(LCD16X2_D4_PIN);

        if ((nibble & 0x02U) != 0U) LCD16X2_D5_PORT |= _BV(LCD16X2_D5_PIN);
        else                        LCD16X2_D5_PORT &= (uint8_t)~_BV(LCD16X2_D5_PIN);

        if ((nibble & 0x04U) != 0U) LCD16X2_D6_PORT |= _BV(LCD16X2_D6_PIN);
        else                        LCD16X2_D6_PORT &= (uint8_t)~_BV(LCD16X2_D6_PIN);

        if ((nibble & 0x08U) != 0U) LCD16X2_D7_PORT |= _BV(LCD16X2_D7_PIN);
        else                        LCD16X2_D7_PORT &= (uint8_t)~_BV(LCD16X2_D7_PIN);
    }
}

void LCD16x2_InitPins(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        LCD16X2_RS_DDR |= _BV(LCD16X2_RS_PIN);
        LCD16X2_EN_DDR |= _BV(LCD16X2_EN_PIN);
        LCD16X2_D4_DDR |= _BV(LCD16X2_D4_PIN);
        LCD16X2_D5_DDR |= _BV(LCD16X2_D5_PIN);
        LCD16X2_D6_DDR |= _BV(LCD16X2_D6_PIN);
        LCD16X2_D7_DDR |= _BV(LCD16X2_D7_PIN);

        LCD16X2_RS_PORT &= (uint8_t)~_BV(LCD16X2_RS_PIN);
        LCD16X2_EN_PORT &= (uint8_t)~_BV(LCD16X2_EN_PIN);
        LCD16X2_D4_PORT &= (uint8_t)~_BV(LCD16X2_D4_PIN);
        LCD16X2_D5_PORT &= (uint8_t)~_BV(LCD16X2_D5_PIN);
        LCD16X2_D6_PORT &= (uint8_t)~_BV(LCD16X2_D6_PIN);
        LCD16X2_D7_PORT &= (uint8_t)~_BV(LCD16X2_D7_PIN);
    }
}

void LCD16x2_Write4Bits(uint8_t nibble, bool is_data)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (is_data)
        {
            LCD16X2_RS_PORT |= _BV(LCD16X2_RS_PIN);
        }
        else
        {
            LCD16X2_RS_PORT &= (uint8_t)~_BV(LCD16X2_RS_PIN);
        }
    }

    lcd16x2_set_data_nibble(nibble & 0x0FU);
    lcd16x2_pulse_enable();
    _delay_us(42); /* Standard HD44780 instruction execution time (~37 us) */
}

void LCD16x2_SendCommand(uint8_t cmd)
{
    LCD16x2_Write4Bits((uint8_t)(cmd >> 4U), false);
    LCD16x2_Write4Bits((uint8_t)(cmd & 0x0FU), false);
}

void LCD16x2_WriteChar(char c)
{
    uint8_t val = (uint8_t)c;
    LCD16x2_Write4Bits((uint8_t)(val >> 4U), true);
    LCD16x2_Write4Bits((uint8_t)(val & 0x0FU), true);
}

void LCD16x2_SetCursor(uint8_t row, uint8_t col)
{
    if (row >= LCD16X2_ROWS || col >= LCD16X2_COLS)
    {
        return;
    }
    LCD16x2_SendCommand((uint8_t)(LCD16X2_CMD_SET_DDRAM_ADDR | (k_row_offsets[row] + col)));
    s_queued_row = row;
    s_queued_col = col;
}

void LCD16x2_WriteString(const char *str)
{
    if (str == NULL)
    {
        return;
    }
    while (*str != '\0')
    {
        LCD16x2_WriteChar(*str++);
    }
}


/* Middleware FIFO Queueing & Non-Blocking Co-routine Engine */
static bool lcd16x2_fifo_push_pair(uint8_t op_type, uint8_t value)
{
    bool ok = false;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if ((LCD16X2_FIFO_CAPACITY - FIFO_Count(&s_lcd_fifo)) >= 2U)
        {
            (void)FIFO_Push(&s_lcd_fifo, op_type);
            (void)FIFO_Push(&s_lcd_fifo, value);
            ok = true;
        }
    }
    return ok;
}

static bool lcd16x2_fifo_pop_pair(uint8_t *op_type, uint8_t *value)
{
    bool ok = false;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (FIFO_Count(&s_lcd_fifo) >= 2U)
        {
            (void)FIFO_Pop(&s_lcd_fifo, op_type);
            (void)FIFO_Pop(&s_lcd_fifo, value);
            ok = true;
        }
    }
    return ok;
}

bool LCD16x2_EnqueueCommand(uint8_t cmd)
{
    s_queued_row = 0xFFU;
    s_queued_col = 0xFFU;
    return lcd16x2_fifo_push_pair(LCD16X2_OP_CMD, cmd);
}

bool LCD16x2_EnqueueChar(char c)
{
    return lcd16x2_fifo_push_pair(LCD16X2_OP_DATA, (uint8_t)c);
}

uint16_t LCD16x2_PendingQueueCount(void)
{
    return FIFO_Count(&s_lcd_fifo) / 2U;
}

void LCD16x2_BufferClear(void)
{
    uint8_t r, c;
    for (r = 0U; r < LCD16X2_ROWS; r++)
    {
        for (c = 0U; c < LCD16X2_COLS; c++)
        {
            s_target_buf[r][c] = ' ';
        }
    }
}

void LCD16x2_InvalidateShadow(void)
{
    uint8_t r, c;
    for (r = 0U; r < LCD16X2_ROWS; r++)
    {
        for (c = 0U; c < LCD16X2_COLS; c++)
        {
            s_shadow_buf[r][c] = '\0';
        }
    }
    s_queued_row = 0xFFU;
    s_queued_col = 0xFFU;
}

void LCD16x2_InitBlocking(void)
{
    uint8_t r, c;

    LCD16x2_InitPins();
    (void)FIFO_Init(&s_lcd_fifo, s_lcd_fifo_storage, LCD16X2_FIFO_CAPACITY);
    LCD16x2_BufferClear();

    _delay_ms(20);
    LCD16x2_Write4Bits(0x03U, false);
    _delay_ms(5);
    LCD16x2_Write4Bits(0x03U, false);
    _delay_ms(1);
    LCD16x2_Write4Bits(0x03U, false);
    LCD16x2_Write4Bits(0x02U, false); /* Switch to 4-bit interface */

    LCD16x2_SendCommand(LCD16X2_CMD_FUNCTION_4BIT_2L);
    LCD16x2_SendCommand(LCD16X2_CMD_DISPLAY_ON);
    LCD16x2_SendCommand(LCD16X2_CMD_ENTRY_MODE_SET);
    LCD16x2_SendCommand(LCD16X2_CMD_CLEAR_DISPLAY);
    _delay_ms(2);

    for (r = 0U; r < LCD16X2_ROWS; r++)
    {
        for (c = 0U; c < LCD16X2_COLS; c++)
        {
            s_shadow_buf[r][c] = ' ';
        }
    }
    s_queued_row = 0U;
    s_queued_col = 0U;
    s_lcd_state = LCD16X2_STATE_READY;
}

void LCD16x2_InitAsync(uint32_t current_tick_ms)
{
    LCD16x2_InitPins();
    (void)FIFO_Init(&s_lcd_fifo, s_lcd_fifo_storage, LCD16X2_FIFO_CAPACITY);
    LCD16x2_BufferClear();
    LCD16x2_InvalidateShadow();
    s_wait_start_ms = current_tick_ms;
    s_lcd_state = LCD16X2_STATE_WAIT_POWER_ON;
}

bool LCD16x2_IsReady(void)
{
    return (s_lcd_state == LCD16X2_STATE_READY);
}

LCD16x2_State_t LCD16x2_GetState(void)
{
    return s_lcd_state;
}

/**
 * Scan differential framebuffer and push changed characters into the middleware FIFO.
 */
static bool lcd16x2_sync_diff_to_fifo(void)
{
    uint8_t r, c;
    bool all_synced = true;

    for (r = 0U; r < LCD16X2_ROWS; r++)
    {
        for (c = 0U; c < LCD16X2_COLS; c++)
        {
            if (s_target_buf[r][c] != s_shadow_buf[r][c])
            {
                /* Need at least 4 bytes (2 pairs) in FIFO if cursor jump is required */
                if ((LCD16X2_FIFO_CAPACITY - FIFO_Count(&s_lcd_fifo)) < 4U)
                {
                    return false;
                }

                if (s_queued_row != r || s_queued_col != c)
                {
                    uint8_t addr_cmd = (uint8_t)(LCD16X2_CMD_SET_DDRAM_ADDR | (k_row_offsets[r] + c));
                    (void)lcd16x2_fifo_push_pair(LCD16X2_OP_CMD, addr_cmd);
                    s_queued_row = r;
                    s_queued_col = c;
                }

                (void)lcd16x2_fifo_push_pair(LCD16X2_OP_DATA, (uint8_t)s_target_buf[r][c]);
                s_shadow_buf[r][c] = s_target_buf[r][c];
                s_queued_col++;
            }
        }
    }

    return all_synced;
}

bool LCD16x2_TaskStep(uint32_t current_tick_ms, uint8_t max_ops_per_step)
{
    uint8_t r, c;
    uint8_t ops_done = 0U;
    uint8_t op_type, op_val;
    bool diff_complete;

    switch (s_lcd_state)
    {
        case LCD16X2_STATE_UNINIT:
            LCD16x2_InitAsync(current_tick_ms);
            return false;

        case LCD16X2_STATE_WAIT_POWER_ON:
            if ((uint32_t)(current_tick_ms - s_wait_start_ms) >= 20UL)
            {
                LCD16x2_Write4Bits(0x03U, false);
                s_wait_start_ms = current_tick_ms;
                s_lcd_state = LCD16X2_STATE_INIT_STEP1_WAIT;
            }
            return false;

        case LCD16X2_STATE_INIT_STEP1_WAIT:
            if ((uint32_t)(current_tick_ms - s_wait_start_ms) >= 5UL)
            {
                LCD16x2_Write4Bits(0x03U, false);
                s_wait_start_ms = current_tick_ms;
                s_lcd_state = LCD16X2_STATE_INIT_STEP2_WAIT;
            }
            return false;

        case LCD16X2_STATE_INIT_STEP2_WAIT:
            if ((uint32_t)(current_tick_ms - s_wait_start_ms) >= 1UL)
            {
                LCD16x2_Write4Bits(0x03U, false);
                LCD16x2_Write4Bits(0x02U, false); /* Enter 4-bit mode */
                s_wait_start_ms = current_tick_ms;
                s_lcd_state = LCD16X2_STATE_INIT_STEP3_WAIT;
            }
            return false;

        case LCD16X2_STATE_INIT_STEP3_WAIT:
            LCD16x2_SendCommand(LCD16X2_CMD_FUNCTION_4BIT_2L);
            LCD16x2_SendCommand(LCD16X2_CMD_DISPLAY_ON);
            LCD16x2_SendCommand(LCD16X2_CMD_ENTRY_MODE_SET);
            LCD16x2_SendCommand(LCD16X2_CMD_CLEAR_DISPLAY);
            s_wait_start_ms = current_tick_ms;
            s_lcd_state = LCD16X2_STATE_CLEAR_WAIT;
            return false;

        case LCD16X2_STATE_CLEAR_WAIT:
            if ((uint32_t)(current_tick_ms - s_wait_start_ms) >= 2UL)
            {
                for (r = 0U; r < LCD16X2_ROWS; r++)
                {
                    for (c = 0U; c < LCD16X2_COLS; c++)
                    {
                        s_shadow_buf[r][c] = ' ';
                    }
                }
                s_queued_row = 0U;
                s_queued_col = 0U;
                s_lcd_state = LCD16X2_STATE_READY;
            }
            return false;

        case LCD16X2_STATE_READY:
        default:
            if (max_ops_per_step == 0U)
            {
                max_ops_per_step = 1U;
            }

            /* 1. Push any changed characters from target_buf into the middleware FIFO */
            diff_complete = lcd16x2_sync_diff_to_fifo();

            /* 2. Drain up to max_ops_per_step operations from the FIFO to the LCD */
            while (ops_done < max_ops_per_step && lcd16x2_fifo_pop_pair(&op_type, &op_val))
            {
                if (op_type == LCD16X2_OP_CMD)
                {
                    LCD16x2_SendCommand(op_val);
                    if (op_val == LCD16X2_CMD_CLEAR_DISPLAY || op_val == LCD16X2_CMD_RETURN_HOME)
                    {
                        s_wait_start_ms = current_tick_ms;
                        s_lcd_state = LCD16X2_STATE_CLEAR_WAIT;
                        return false;
                    }
                }
                else
                {
                    LCD16x2_WriteChar((char)op_val);
                }
                ops_done++;
            }

            return diff_complete && (FIFO_Count(&s_lcd_fifo) == 0U);
    }
}


/* Framebuffer Formatting Functions */
void LCD16x2_BufferPutChar(uint8_t row, uint8_t col, char c)
{
    if (row < LCD16X2_ROWS && col < LCD16X2_COLS)
    {
        s_target_buf[row][col] = c;
    }
}

void LCD16x2_BufferPutString(uint8_t row, uint8_t col, const char *str)
{
    if (row >= LCD16X2_ROWS || str == NULL)
    {
        return;
    }
    while (*str != '\0' && col < LCD16X2_COLS)
    {
        s_target_buf[row][col++] = *str++;
    }
}

void LCD16x2_BufferWriteLinePadded(uint8_t row, const char *str)
{
    uint8_t col = 0U;
    if (row >= LCD16X2_ROWS)
    {
        return;
    }
    if (str != NULL)
    {
        while (*str != '\0' && col < LCD16X2_COLS)
        {
            s_target_buf[row][col++] = *str++;
        }
    }
    while (col < LCD16X2_COLS)
    {
        s_target_buf[row][col++] = ' ';
    }
}

static void lcd16x2_write_two_digits(char *dst, uint8_t value)
{
    dst[0] = (char)('0' + ((value / 10U) % 10U));
    dst[1] = (char)('0' + (value % 10U));
}

void LCD16x2_FormatClockScreen(uint8_t date,
                               uint8_t month,
                               uint8_t year_2digit,
                               const char *dow_str,
                               uint8_t hours,
                               uint8_t minutes,
                               uint8_t seconds,
                               const char *led_mode_tag,
                               bool rtc_error)
{
    char line0[LCD16X2_COLS + 1U];
    char line1[LCD16X2_COLS + 1U];
    uint8_t i;

    for (i = 0U; i < LCD16X2_COLS; i++)
    {
        line0[i] = ' ';
        line1[i] = ' ';
    }
    line0[LCD16X2_COLS] = '\0';
    line1[LCD16X2_COLS] = '\0';

    /* Row 0 (cols 0..15): "26/08/2026  WED " */
    lcd16x2_write_two_digits(&line0[0], date);
    line0[2] = '/';
    lcd16x2_write_two_digits(&line0[3], month);
    line0[5] = '/';
    line0[6] = '2';
    line0[7] = '0';
    lcd16x2_write_two_digits(&line0[8], year_2digit);

    if (dow_str != NULL)
    {
        for (i = 0U; i < 3U && dow_str[i] != '\0'; i++)
        {
            line0[12U + i] = dow_str[i];
        }
    }

    if (!rtc_error)
    {
        /* Row 1 (cols 0..15): "14:30:05   L:BLK" */
        lcd16x2_write_two_digits(&line1[0], hours);
        line1[2] = ':';
        lcd16x2_write_two_digits(&line1[3], minutes);
        line1[5] = ':';
        lcd16x2_write_two_digits(&line1[6], seconds);

        line1[11] = 'L';
        line1[12] = ':';
        if (led_mode_tag != NULL)
        {
            for (i = 0U; i < 3U && led_mode_tag[i] != '\0'; i++)
            {
                line1[13U + i] = led_mode_tag[i];
            }
        }
    }
    else
    {
        /* When DS3231 is disconnected/faulted, Row 2 displays "RTC ERROR" (Section 3.1). */
        const char *err_msg = "RTC ERROR";
        for (i = 0U; err_msg[i] != '\0' && i < LCD16X2_COLS; i++)
        {
            line1[i] = err_msg[i];
        }
    }

    LCD16x2_BufferWriteLinePadded(0U, line0);
    LCD16x2_BufferWriteLinePadded(1U, line1);
}

