#include "bsp_lcd.h"

#include <stddef.h>
#include "../board.h"
#include "../../M3_Driver/Component/DIO/do.h"
#include "../../M3_Driver/Devices/LCD16x2/lcd16x2.h"
#include "../../M3_Driver/Devices/DS3231/ds3231.h"

/* Board GPIO pin descriptors on Port A (PA0=RS, PA1=EN, PA4..PA7=D4..D7) */
static do_t s_lcd_pins[6] = {
    { .port = DO_PORT_A, .pin = LCD_RS_PIN, .bStatus = false },
    { .port = DO_PORT_A, .pin = LCD_EN_PIN, .bStatus = false },
    { .port = DO_PORT_A, .pin = LCD_D4_PIN, .bStatus = false },
    { .port = DO_PORT_A, .pin = LCD_D5_PIN, .bStatus = false },
    { .port = DO_PORT_A, .pin = LCD_D6_PIN, .bStatus = false },
    { .port = DO_PORT_A, .pin = LCD_D7_PIN, .bStatus = false }
};

static bool bsp_lcd_init_gpio(void)
{
    uint8_t i;
    for (i = 0U; i < 6U; i++)
    {
        s_lcd_pins[i].bStatus = false;
        if (!do_init(&s_lcd_pins[i]))
        {
            return false;
        }
    }
    return true;
}

bool BSP_LCD_Init(uint32_t current_tick_ms)
{
    if (!bsp_lcd_init_gpio())
    {
        return false;
    }
    LCD16x2_InitAsync(current_tick_ms);
    return true;
}

bool BSP_LCD_InitBlocking(void)
{
    if (!bsp_lcd_init_gpio())
    {
        return false;
    }
    LCD16x2_InitBlocking();
    return true;
}

bool BSP_LCD_IsReady(void)
{
    return LCD16x2_IsReady();
}

bool BSP_LCD_TaskStep(uint32_t current_tick_ms)
{
    return LCD16x2_TaskStep(current_tick_ms, BSP_LCD_OPS_PER_STEP);
}

void BSP_LCD_Clear(void)
{
    LCD16x2_BufferClear();
}

void BSP_LCD_ForceRedraw(void)
{
    LCD16x2_InvalidateShadow();
}

void BSP_LCD_PutChar(uint8_t row, uint8_t col, char c)
{
    LCD16x2_BufferPutChar(row, col, c);
}

void BSP_LCD_PutString(uint8_t row, uint8_t col, const char *str)
{
    LCD16x2_BufferPutString(row, col, str);
}

void BSP_LCD_WriteLine(uint8_t row, const char *str)
{
    LCD16x2_BufferWriteLinePadded(row, str);
}

void BSP_LCD_UpdateClockView(const DS3231_DateTimeType *dt,
                             const char *led_mode_tag,
                             bool rtc_error)
{
    if (dt != NULL)
    {
        LCD16x2_FormatClockScreen(dt->date,
                                  dt->month,
                                  dt->year,
                                  DS3231_DayToString(dt->day),
                                  dt->hours,
                                  dt->minutes,
                                  dt->seconds,
                                  led_mode_tag,
                                  rtc_error);
    }
    else
    {
        LCD16x2_FormatClockScreen(0U,
                                  0U,
                                  0U,
                                  "---",
                                  0U,
                                  0U,
                                  0U,
                                  led_mode_tag,
                                  true);
    }
}

