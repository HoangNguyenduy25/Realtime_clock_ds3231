#include "app_temp.h"

#include <stddef.h>
#include "../M1_Sys/Scheduler/scheduler.h"
#include "../M1_Sys/SystemData/system_data.h"
#include "../M2_BSP/DS3231/bsp_ds3231.h"

/**
 * Co-routine states for non-blocking periodic temperature sampling.
 */
typedef enum
{
    APP_TEMP_CO_WAIT_INTERVAL = 0,
    APP_TEMP_CO_READ_SENSOR
} AppTemp_CoState_t;

static AppTemp_CoState_t s_temp_co_state = APP_TEMP_CO_WAIT_INTERVAL;
static uint32_t          s_last_temp_read_ms = 0UL;

bool AppTemp_Init(void)
{
    DS3231_TempType temp;

    s_last_temp_read_ms = SCH_SystemTick();
    s_temp_co_state     = APP_TEMP_CO_WAIT_INTERVAL;

    if (BSP_DS3231_ReadTemperature(&temp))
    {
        SystemData_UpdateTemperature(&temp);
        return true;
    }

    SystemData_SetRtcError(true);
    return false;
}

AppTemp_Status_t AppTemp_ReadNow(DS3231_TempType *temp)
{
    DS3231_TempType local_temp;

    if (!BSP_DS3231_ReadTemperature(&local_temp))
    {
        SystemData_SetRtcError(true);
        return APP_TEMP_ERR_RTC_FAIL;
    }

    SystemData_UpdateTemperature(&local_temp);
    s_last_temp_read_ms = SCH_SystemTick();

    if (temp != NULL)
    {
        *temp = local_temp;
    }
    return APP_TEMP_OK;
}

AppTemp_Status_t AppTemp_GetTemperature(DS3231_TempType *temp)
{
    if (temp == NULL)
    {
        return APP_TEMP_ERR_RTC_FAIL;
    }

    return AppTemp_ReadNow(temp);
}

void AppTemp_FormatString(const DS3231_TempType *temp, char *out_buf, uint8_t buf_size)
{
    uint8_t idx = 0U;
    uint8_t abs_int;
    uint8_t hundreds, tens, ones;

    if (out_buf == NULL || buf_size < 7U)
    {
        if (out_buf != NULL && buf_size > 0U)
        {
            out_buf[0] = '\0';
        }
        return;
    }

    if (temp == NULL)
    {
        out_buf[0] = '0';
        out_buf[1] = '.';
        out_buf[2] = '0';
        out_buf[3] = '0';
        out_buf[4] = '\0';
        return;
    }

    if (temp->negative)
    {
        out_buf[idx++] = '-';
        abs_int = (temp->integer < 0) ? (uint8_t)(-temp->integer) : (uint8_t)temp->integer;
    }
    else
    {
        abs_int = (uint8_t)temp->integer;
    }

    hundreds = abs_int / 100U;
    tens     = (abs_int / 10U) % 10U;
    ones     = abs_int % 10U;

    if (hundreds > 0U)
    {
        out_buf[idx++] = (char)('0' + hundreds);
        out_buf[idx++] = (char)('0' + tens);
    }
    else if (tens > 0U)
    {
        out_buf[idx++] = (char)('0' + tens);
    }
    out_buf[idx++] = (char)('0' + ones);

    out_buf[idx++] = '.';
    out_buf[idx++] = (char)('0' + ((temp->frac_hundredths / 10U) % 10U));
    out_buf[idx++] = (char)('0' + (temp->frac_hundredths % 10U));
    out_buf[idx]   = '\0';
}

void AppTemp_Task(void)
{
    uint32_t now_ms = SCH_SystemTick();

    switch (s_temp_co_state)
    {
        case APP_TEMP_CO_WAIT_INTERVAL:
            if ((uint32_t)(now_ms - s_last_temp_read_ms) >= (uint32_t)APP_TEMP_POLL_PERIOD_MS)
            {
                s_temp_co_state = APP_TEMP_CO_READ_SENSOR;
            }
            return; /* Co-routine yield */

        case APP_TEMP_CO_READ_SENSOR:
        default:
        {
            DS3231_TempType temp;
            s_last_temp_read_ms = now_ms;
            if (BSP_DS3231_ReadTemperature(&temp))
            {
                SystemData_UpdateTemperature(&temp);
            }
            else
            {
                SystemData_SetRtcError(true);
            }
            s_temp_co_state = APP_TEMP_CO_WAIT_INTERVAL;
            return; /* Co-routine yield */
        }
    }
}

