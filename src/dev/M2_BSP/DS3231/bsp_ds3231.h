#ifndef BSP_DS3231_H_
#define BSP_DS3231_H_

#include <stdbool.h>
#include <stdint.h>

#include "../board.h"
#include "../../M3_Driver/Devices/DS3231/ds3231.h"

/**
 * Initialize hardware TWI (PC0=SCL, PC1=SDA at 100 kHz from board.h)
 * and configure the DS3231 RTC (24h mode, battery-backed oscillator enabled).
 */
bool BSP_DS3231_Init(void);

/**
 * Probe the DS3231 on the I2C bus (address 0x68) and update the fault state.
 */
bool BSP_DS3231_Probe(void);

/**
 * Returns true if the last DS3231 transaction failed (NACK or bus timeout).
 * Used to trigger "RTC ERROR" on the LCD and "-ERR,E05,RTC ERROR" on UART.
 */
bool BSP_DS3231_HasFault(void);

/**
 * Read/write full Date and Time (7 registers 0x00..0x06 in a single atomic I2C burst).
 */
bool BSP_DS3231_ReadDateTime(DS3231_DateTimeType *dt);
bool BSP_DS3231_SetDateTime(const DS3231_DateTimeType *dt);

/**
 * Read/write Time only (3 registers 0x00..0x02: seconds, minutes, hours).
 */
bool BSP_DS3231_ReadTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds);
bool BSP_DS3231_SetTime(uint8_t hours, uint8_t minutes, uint8_t seconds);

/**
 * Read/write Date only (4 registers 0x03..0x06: day-of-week, date, month, year).
 */
bool BSP_DS3231_ReadDate(uint8_t *date, uint8_t *month, uint8_t *year, uint8_t *day);
bool BSP_DS3231_SetDate(uint8_t date, uint8_t month, uint8_t year, uint8_t day);

/**
 * Read internal temperature sensor (registers 0x11..0x12, 0.25 C resolution).
 */
bool BSP_DS3231_ReadTemperature(DS3231_TempType *temp);

/**
 * Configure, disable, or poll Alarm 1.
 */
bool BSP_DS3231_SetAlarm1(const DS3231_AlarmType *alarm, bool enable_interrupt);
bool BSP_DS3231_DisableAlarm1(void);
bool BSP_DS3231_CheckAndClearAlarm1(bool *triggered);

#endif /* BSP_DS3231_H_ */

