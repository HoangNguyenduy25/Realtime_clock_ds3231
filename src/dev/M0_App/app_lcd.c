#include "app_lcd.h"

#include <stddef.h>
#include "../M1_Sys/Scheduler/scheduler.h"
#include "../M1_Sys/SystemData/system_data.h"
#include "../M2_BSP/LCD/bsp_lcd.h"

/**
 * Co-routine states for non-blocking LCD initialization and differential rendering.
 */
typedef enum
{
    APP_LCD_CO_INIT_HW = 0,
    APP_LCD_CO_WAIT_DIRTY,
    APP_LCD_CO_FORMAT_BUFFER,
    APP_LCD_CO_FLUSH_DIFF
} AppLcd_CoState_t;

static AppLcd_CoState_t s_lcd_co_state = APP_LCD_CO_INIT_HW;
static uint32_t         s_last_refresh_ms = 0UL;

bool AppLcd_Init(void)
{
    uint32_t now_ms = SCH_SystemTick();
    bool ok = BSP_LCD_Init(now_ms);

    s_last_refresh_ms = now_ms;
    s_lcd_co_state    = APP_LCD_CO_INIT_HW;
    SystemData_MarkDisplayDirty();
    return ok;
}

void AppLcd_RequestRedraw(void)
{
    BSP_LCD_ForceRedraw();
    SystemData_MarkDisplayDirty();
}

bool AppLcd_IsReady(void)
{
    return BSP_LCD_IsReady();
}

void AppLcd_Task(void)
{
    uint32_t now_ms = SCH_SystemTick();

    switch (s_lcd_co_state)
    {
        case APP_LCD_CO_INIT_HW:
            /* Advance non-blocking 4-bit HD44780 initialization state machine */
            (void)BSP_LCD_TaskStep(now_ms);
            if (BSP_LCD_IsReady())
            {
                s_last_refresh_ms = now_ms;
                s_lcd_co_state    = APP_LCD_CO_FORMAT_BUFFER;
            }
            return; /* Co-routine yield */

        case APP_LCD_CO_WAIT_DIRTY:
            /* Wait until SystemData marks display dirty (time tick, LED change, or RTC fault)
             * or 250 ms periodic check elapses. */
            if (SystemData_IsDisplayDirty() ||
                (uint32_t)(now_ms - s_last_refresh_ms) >= (uint32_t)APP_LCD_REFRESH_PERIOD_MS)
            {
                s_last_refresh_ms = now_ms;
                s_lcd_co_state    = APP_LCD_CO_FORMAT_BUFFER;
            }
            else
            {
                /* Drain any remaining queued bytes in the middleware FIFO */
                (void)BSP_LCD_TaskStep(now_ms);
            }
            return; /* Co-routine yield */

        case APP_LCD_CO_FORMAT_BUFFER:
        {
            DS3231_DateTimeType dt;
            bool rtc_error;
            const char *led_tag;

            rtc_error = !SystemData_GetDateTime(&dt) || SystemData_HasRtcError();
            led_tag   = SystemData_GetLedLcdTag();

            /* Format Row 0 ("dd/mm/yyyy  DOW ") and Row 1 ("hh:mm:ss   L:xxx" or "RTC ERROR") */
            BSP_LCD_UpdateClockView(&dt, led_tag, rtc_error);
            SystemData_ClearDisplayDirty();

            s_lcd_co_state = APP_LCD_CO_FLUSH_DIFF;
            return; /* Co-routine yield before hardware bus transfer */
        }

        case APP_LCD_CO_FLUSH_DIFF:
        default:
            /* Transmit up to BSP_LCD_OPS_PER_STEP changed characters (~170 us) per tick */
            if (BSP_LCD_TaskStep(now_ms))
            {
                s_lcd_co_state = APP_LCD_CO_WAIT_DIRTY;
            }
            return; /* Co-routine yield */
    }
}

