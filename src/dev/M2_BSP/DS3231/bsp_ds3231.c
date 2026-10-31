#include "bsp_ds3231.h"

#include <avr/io.h>
#include <stddef.h>

#include "../board.h"
#include "../../M3_Driver/Component/I2C/i2c.h"
#include "../../M3_Driver/Devices/DS3231/ds3231.h"

static bool s_rtc_fault = false;

static bool bsp_ds3231_update_status(bool ok)
{
    if (!ok || DS3231_HasBusFault())
    {
        s_rtc_fault = true;
        return false;
    }
    s_rtc_fault = false;
    return true;
}

bool BSP_DS3231_Init(void)
{
    /* Configure PC0 (SCL) and PC1 (SDA) as inputs with pull-ups enabled per board.h */
    TWI_SCL_DDR  &= (uint8_t)~_BV(TWI_SCL_PIN);
    TWI_SDA_DDR  &= (uint8_t)~_BV(TWI_SDA_PIN);
    TWI_SCL_PORT |= _BV(TWI_SCL_PIN);
    TWI_SDA_PORT |= _BV(TWI_SDA_PIN);

    if (!I2C_AVR_Init(F_CPU, TWI_SCL_FREQ_HZ))
    {
        s_rtc_fault = true;
        return false;
    }

    return bsp_ds3231_update_status(DS3231_Init());
}

bool BSP_DS3231_Probe(void)
{
    return bsp_ds3231_update_status(DS3231_IsConnected());
}

bool BSP_DS3231_HasFault(void)
{
    return s_rtc_fault || DS3231_HasBusFault();
}

bool BSP_DS3231_ReadDateTime(DS3231_DateTimeType *dt)
{
    return bsp_ds3231_update_status(DS3231_ReadDateTime(dt));
}

bool BSP_DS3231_SetDateTime(const DS3231_DateTimeType *dt)
{
    return bsp_ds3231_update_status(DS3231_SetDateTime(dt));
}

bool BSP_DS3231_ReadTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds)
{
    return bsp_ds3231_update_status(DS3231_ReadTime(hours, minutes, seconds));
}

bool BSP_DS3231_SetTime(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    return bsp_ds3231_update_status(DS3231_SetTime(hours, minutes, seconds));
}

bool BSP_DS3231_ReadDate(uint8_t *date, uint8_t *month, uint8_t *year, uint8_t *day)
{
    return bsp_ds3231_update_status(DS3231_ReadDate(date, month, year, day));
}

bool BSP_DS3231_SetDate(uint8_t date, uint8_t month, uint8_t year, uint8_t day)
{
    return bsp_ds3231_update_status(DS3231_SetDate(date, month, year, day));
}

bool BSP_DS3231_ReadTemperature(DS3231_TempType *temp)
{
    return bsp_ds3231_update_status(DS3231_ReadTemperature(temp));
}

bool BSP_DS3231_SetAlarm1(const DS3231_AlarmType *alarm, bool enable_interrupt)
{
    return bsp_ds3231_update_status(DS3231_SetAlarm1(alarm, enable_interrupt));
}

bool BSP_DS3231_DisableAlarm1(void)
{
    return bsp_ds3231_update_status(DS3231_DisableAlarm1());
}

bool BSP_DS3231_CheckAndClearAlarm1(bool *triggered)
{
    return bsp_ds3231_update_status(DS3231_CheckAndClearAlarm1(triggered));
}

