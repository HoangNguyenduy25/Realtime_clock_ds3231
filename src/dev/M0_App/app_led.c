#include "app_led.h"

#include <stddef.h>
#include "../M1_Sys/Scheduler/scheduler.h"
#include "../M1_Sys/SystemData/system_data.h"
#include "../M2_BSP/LED/bsp_led.h"

/**
 * Co-routine state machine for non-blocking LED control.
 */
typedef enum
{
    APP_LED_CO_INIT = 0,
    APP_LED_CO_STEADY,
    APP_LED_CO_BLINK_ON_WAIT,
    APP_LED_CO_BLINK_OFF_WAIT
} AppLed_CoState_t;

static AppLed_CoState_t s_led_co_state = APP_LED_CO_INIT;
static uint16_t         s_active_period_ms = SYSTEM_DATA_BLINK_DEFAULT_MS;

static void app_led_apply_pin(bool on)
{
    BSP_LED_Set(on);
    SystemData_SetLedPinState(on);
}

bool AppLed_Init(void)
{
    bool ok = BSP_LED_Init();
    SystemData_SetLedModeOff();
    app_led_apply_pin(false);
    s_active_period_ms = SystemData_GetLedBlinkPeriod();
    s_led_co_state     = APP_LED_CO_STEADY;
    return ok;
}

void AppLed_SetOn(void)
{
    SCH_TIM_Start(SCH_TIM_LED, 0UL);
    SystemData_SetLedModeOn();
    app_led_apply_pin(true);
    s_led_co_state = APP_LED_CO_STEADY;
}

void AppLed_SetOff(void)
{
    SCH_TIM_Start(SCH_TIM_LED, 0UL);
    SystemData_SetLedModeOff();
    app_led_apply_pin(false);
    s_led_co_state = APP_LED_CO_STEADY;
}

AppLed_Status_t AppLed_SetBlink(uint16_t period_ms)
{
    uint16_t half_on_ms;

    if (period_ms < SYSTEM_DATA_BLINK_MIN_MS || period_ms > SYSTEM_DATA_BLINK_MAX_MS)
    {
        return APP_LED_ERR_OUT_OF_RANGE;
    }

    if (!SystemData_SetLedModeBlink(period_ms))
    {
        return APP_LED_ERR_OUT_OF_RANGE;
    }

    /* Start the ON half-cycle (T/2 ms) immediately using the Scheduler soft timer */
    s_active_period_ms = period_ms;
    half_on_ms         = period_ms / 2U;
    app_led_apply_pin(true);
    SCH_TIM_Start(SCH_TIM_LED, (uint32_t)half_on_ms);
    s_led_co_state = APP_LED_CO_BLINK_ON_WAIT;

    return APP_LED_OK;
}

AppLed_Status_t AppLed_SetBlinkFromStr(const char *period_str, uint16_t *out_period_ms)
{
    uint32_t acc = 0UL;
    uint8_t  i = 0U;

    if (period_str == NULL || period_str[0] == '\0')
    {
        return APP_LED_ERR_BAD_FORMAT;
    }

    /* Section 5.2.5: E04 if non-integer, E03 if integer outside 10..5000 */
    if (period_str[0] == '-')
    {
        for (i = 1U; period_str[i] != '\0'; i++)
        {
            if (period_str[i] < '0' || period_str[i] > '9')
            {
                return APP_LED_ERR_BAD_FORMAT;
            }
        }
        return APP_LED_ERR_OUT_OF_RANGE;
    }

    for (i = 0U; period_str[i] != '\0'; i++)
    {
        if (period_str[i] < '0' || period_str[i] > '9')
        {
            return APP_LED_ERR_BAD_FORMAT;
        }
        acc = (acc * 10UL) + (uint32_t)(period_str[i] - '0');
        if (acc > 60000UL)
        {
            acc = 60000UL;
        }
    }

    if (acc < SYSTEM_DATA_BLINK_MIN_MS || acc > SYSTEM_DATA_BLINK_MAX_MS)
    {
        return APP_LED_ERR_OUT_OF_RANGE;
    }

    if (out_period_ms != NULL)
    {
        *out_period_ms = (uint16_t)acc;
    }

    return AppLed_SetBlink((uint16_t)acc);
}

SystemData_LedMode AppLed_GetMode(void)
{
    return SystemData_GetLedMode();
}

uint16_t AppLed_GetBlinkPeriod(void)
{
    return SystemData_GetLedBlinkPeriod();
}

const char *AppLed_GetLcdTag(void)
{
    return SystemData_GetLedLcdTag();
}

void AppLed_Task(void)
{
    SystemData_LedMode mode = SystemData_GetLedMode();
    uint16_t           period_ms = SystemData_GetLedBlinkPeriod();

    /* Synchronize co-routine state if SystemData mode/period was updated */
    if (mode != SYS_LED_MODE_BLINK && s_led_co_state != APP_LED_CO_STEADY)
    {
        s_led_co_state = APP_LED_CO_STEADY;
    }
    else if (mode == SYS_LED_MODE_BLINK &&
             (s_led_co_state == APP_LED_CO_STEADY || s_active_period_ms != period_ms))
    {
        s_active_period_ms = period_ms;
        app_led_apply_pin(true);
        SCH_TIM_Start(SCH_TIM_LED, (uint32_t)(period_ms / 2U));
        s_led_co_state = APP_LED_CO_BLINK_ON_WAIT;
        return; /* Co-routine yield */
    }

    switch (s_led_co_state)
    {
        case APP_LED_CO_INIT:
            (void)AppLed_Init();
            return; /* Co-routine yield */

        case APP_LED_CO_STEADY:
            if (mode == SYS_LED_MODE_ON)
            {
                app_led_apply_pin(true);
            }
            else
            {
                app_led_apply_pin(false);
            }
            return; /* Co-routine yield */

        case APP_LED_CO_BLINK_ON_WAIT:
            if (SCH_TIM_isCompleted(SCH_TIM_LED) == TRUE)
            {
                uint16_t on_ms  = s_active_period_ms / 2U;
                uint16_t off_ms = s_active_period_ms - on_ms;
                app_led_apply_pin(false);
                SCH_TIM_Start(SCH_TIM_LED, (uint32_t)off_ms);
                s_led_co_state = APP_LED_CO_BLINK_OFF_WAIT;
            }
            return; /* Co-routine yield */

        case APP_LED_CO_BLINK_OFF_WAIT:
        default:
            if (SCH_TIM_isCompleted(SCH_TIM_LED) == TRUE)
            {
                uint16_t on_ms = s_active_period_ms / 2U;
                app_led_apply_pin(true);
                SCH_TIM_Start(SCH_TIM_LED, (uint32_t)on_ms);
                s_led_co_state = APP_LED_CO_BLINK_ON_WAIT;
            }
            return; /* Co-routine yield */
    }
}

