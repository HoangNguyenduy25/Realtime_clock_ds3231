#include "ds3231.h"

#include <avr/io.h>
#include <stddef.h>
#include <util/twi.h>


#define F_CPU 8000000UL


#define DS3231_TWI_CLOCK_HZ   100000UL
#define DS3231_TWI_WAIT_LIMIT 10000U

static bool s_twi_bus_fault = false;

/* Day-of-week 3-letter uppercase strings indexed by 1..7 (1 = SUN .. 7 = SAT). */
static const char *const s_dow_names[8] = {
    "ERR",
    "SUN",
    "MON",
    "TUE",
    "WED",
    "THU",
    "FRI",
    "SAT"
};

/* Internal AVR Hardware TWI Helpers */
static bool ds3231_twi_wait(void)
{
    uint16_t remaining = DS3231_TWI_WAIT_LIMIT;
    while ((TWCR & _BV(TWINT)) == 0U && remaining-- != 0U)
    {
        /* Bounded wait for TWI operation to complete */
    }
    if ((TWCR & _BV(TWINT)) != 0U)
    {
        return true;
    }

    /* Recover TWI hardware on bus lockup/timeout */
    TWCR = 0U;
    TWCR = _BV(TWEN);
    s_twi_bus_fault = true;
    return false;
}

static bool ds3231_twi_start(uint8_t address_rw)
{
    uint8_t status;

    TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
    if (!ds3231_twi_wait())
    {
        return false;
    }

    status = TW_STATUS;
    if (status != TW_START && status != TW_REP_START)
    {
        return false;
    }

    TWDR = address_rw;
    TWCR = _BV(TWINT) | _BV(TWEN);
    if (!ds3231_twi_wait())
    {
        return false;
    }

    status = TW_STATUS;
    return (status == TW_MT_SLA_ACK) || (status == TW_MR_SLA_ACK);
}

static bool ds3231_twi_write(uint8_t value)
{
    TWDR = value;
    TWCR = _BV(TWINT) | _BV(TWEN);
    if (!ds3231_twi_wait())
    {
        return false;
    }
    return (TW_STATUS == TW_MT_DATA_ACK);
}

static bool ds3231_twi_read(uint8_t *value, bool ack)
{
    uint8_t expected_status;

    if (value == NULL)
    {
        return false;
    }

    TWCR = _BV(TWINT) | _BV(TWEN) | (ack ? _BV(TWEA) : 0U);
    if (!ds3231_twi_wait())
    {
        return false;
    }

    expected_status = ack ? TW_MR_DATA_ACK : TW_MR_DATA_NACK;
    if (TW_STATUS != expected_status)
    {
        return false;
    }

    *value = TWDR;
    return true;
}

static void ds3231_twi_stop(void)
{
    TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO);
}

static bool ds3231_read_block(uint8_t reg, uint8_t *data, uint8_t length)
{
    uint8_t i;
    bool ok;

    if (data == NULL || length == 0U)
    {
        return false;
    }

    s_twi_bus_fault = false;
    ok = ds3231_twi_start((uint8_t)((DS3231_I2C_ADDR << 1U) | TW_WRITE)) &&
         ds3231_twi_write(reg) &&
         ds3231_twi_start((uint8_t)((DS3231_I2C_ADDR << 1U) | TW_READ));

    for (i = 0U; ok && i < length; i++)
    {
        ok = ds3231_twi_read(&data[i], (i + 1U) < length);
    }

    if (!s_twi_bus_fault)
    {
        ds3231_twi_stop();
    }
    if (!ok)
    {
        s_twi_bus_fault = true;
    }
    return ok;
}

static bool ds3231_write_block(uint8_t reg, const uint8_t *data, uint8_t length)
{
    uint8_t i;
    bool ok;

    if (data == NULL || length == 0U)
    {
        return false;
    }

    s_twi_bus_fault = false;
    ok = ds3231_twi_start((uint8_t)((DS3231_I2C_ADDR << 1U) | TW_WRITE)) &&
         ds3231_twi_write(reg);

    for (i = 0U; ok && i < length; i++)
    {
        ok = ds3231_twi_write(data[i]);
    }

    if (!s_twi_bus_fault)
    {
        ds3231_twi_stop();
    }
    if (!ok)
    {
        s_twi_bus_fault = true;
    }
    return ok;
}

static bool ds3231_read_reg(uint8_t reg, uint8_t *value)
{
    return ds3231_read_block(reg, value, 1U);
}

static bool ds3231_write_reg(uint8_t reg, uint8_t value)
{
    return ds3231_write_block(reg, &value, 1U);
}


/* Conversion & Validation Helpers */
static char ds3231_toupper(char c)
{
    if (c >= 'a' && c <= 'z')
    {
        return (char)(c - ('a' - 'A'));
    }
    return c;
}

uint8_t DS3231_BcdToDec(uint8_t bcd)
{
    return (uint8_t)(((bcd >> 4U) * 10U) + (bcd & 0x0FU));
}

uint8_t DS3231_DecToBcd(uint8_t dec)
{
    return (uint8_t)(((dec / 10U) << 4U) | (dec % 10U));
}

bool DS3231_IsLeapYear(uint16_t full_year)
{
    if ((full_year % 400U) == 0U)
    {
        return true;
    }
    if ((full_year % 100U) == 0U)
    {
        return false;
    }
    return (full_year % 4U) == 0U;
}

uint8_t DS3231_DaysInMonth(uint8_t month, uint16_t full_year)
{
    static const uint8_t k_days_in_month[12] = {
        31U, 28U, 31U, 30U, 31U, 30U, 31U, 31U, 30U, 31U, 30U, 31U
    };

    if (month < 1U || month > 12U)
    {
        return 0U;
    }
    if (month == 2U && DS3231_IsLeapYear(full_year))
    {
        return 29U;
    }
    return k_days_in_month[month - 1U];
}

bool DS3231_IsValidTime(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    return (hours <= 23U) && (minutes <= 59U) && (seconds <= 59U);
}

bool DS3231_IsValidDateRange(uint8_t date, uint8_t month, uint16_t full_year, uint8_t day)
{
    return (date >= 1U && date <= 31U) &&
           (month >= 1U && month <= 12U) &&
           (full_year >= 2000U && full_year <= 2099U) &&
           (day >= DS3231_DOW_SUN && day <= DS3231_DOW_SAT);
}

bool DS3231_IsValidCalendarDate(uint8_t date, uint8_t month, uint16_t full_year)
{
    uint8_t max_days;

    if (month < 1U || month > 12U || full_year < 2000U || full_year > 2099U)
    {
        return false;
    }
    max_days = DS3231_DaysInMonth(month, full_year);
    return (date >= 1U && date <= max_days);
}

const char *DS3231_DayToString(uint8_t day)
{
    if (day >= DS3231_DOW_SUN && day <= DS3231_DOW_SAT)
    {
        return s_dow_names[day];
    }
    return s_dow_names[0];
}

uint8_t DS3231_StringToDay(const char *str)
{
    uint8_t i;
    char c0, c1, c2;

    if (str == NULL || str[0] == '\0' || str[1] == '\0' || str[2] == '\0' || str[3] != '\0')
    {
        return DS3231_DOW_INVALID;
    }

    c0 = ds3231_toupper(str[0]);
    c1 = ds3231_toupper(str[1]);
    c2 = ds3231_toupper(str[2]);

    for (i = DS3231_DOW_SUN; i <= DS3231_DOW_SAT; i++)
    {
        if (c0 == s_dow_names[i][0] &&
            c1 == s_dow_names[i][1] &&
            c2 == s_dow_names[i][2])
        {
            return i;
        }
    }
    return DS3231_DOW_INVALID;
}

/* Public DS3231 Driver API */
bool DS3231_Init(void)
{
    uint8_t ctrl = 0U;
    uint8_t stat = 0U;

    /* Configure AVR TWI hardware if not already initialized */
    if ((TWCR & _BV(TWEN)) == 0U)
    {
        DDRC  &= (uint8_t)~(_BV(PC0) | _BV(PC1));
        PORTC |= _BV(PC0) | _BV(PC1);
        TWSR  &= (uint8_t)~0x03U; /* Prescaler = 1 */
        TWBR   = (uint8_t)(((F_CPU / DS3231_TWI_CLOCK_HZ) - 16UL) / 2UL);
        TWCR   = _BV(TWEN);
    }

    if (!DS3231_IsConnected())
    {
        return false;
    }

    /* Ensure oscillator runs on battery backup (EOSC = 0) and INTCN = 1. */
    if (!ds3231_read_reg(DS3231_REG_CONTROL, &ctrl))
    {
        return false;
    }
    ctrl &= (uint8_t)~DS3231_CTRL_EOSC;
    ctrl |= DS3231_CTRL_INTCN;
    if (!ds3231_write_reg(DS3231_REG_CONTROL, ctrl))
    {
        return false;
    }

    /* Clear Oscillator Stop Flag (OSF) if set. */
    if (!ds3231_read_reg(DS3231_REG_STATUS, &stat))
    {
        return false;
    }
    if ((stat & DS3231_STAT_OSF) != 0U)
    {
        stat &= (uint8_t)~DS3231_STAT_OSF;
        if (!ds3231_write_reg(DS3231_REG_STATUS, stat))
        {
            return false;
        }
    }

    return true;
}

bool DS3231_IsConnected(void)
{
    bool ack;
    s_twi_bus_fault = false;
    ack = ds3231_twi_start((uint8_t)((DS3231_I2C_ADDR << 1U) | TW_WRITE));
    if (!s_twi_bus_fault)
    {
        ds3231_twi_stop();
    }
    if (!ack)
    {
        s_twi_bus_fault = true;
    }
    return ack;
}

bool DS3231_HasBusFault(void)
{
    return s_twi_bus_fault;
}

static uint8_t ds3231_decode_hours24(uint8_t raw_hours)
{
    if ((raw_hours & 0x40U) != 0U)
    {
        uint8_t hour12 = DS3231_BcdToDec(raw_hours & 0x1FU);
        bool is_pm = (raw_hours & 0x20U) != 0U;
        if (hour12 == 12U)
        {
            hour12 = 0U;
        }
        return is_pm ? (uint8_t)(hour12 + 12U) : hour12;
    }
    return DS3231_BcdToDec(raw_hours & 0x3FU);
}

bool DS3231_ReadDateTime(DS3231_DateTimeType *dt)
{
    uint8_t raw[7];

    if (dt == NULL)
    {
        return false;
    }

    /* Read all 7 time/date registers in one continuous I2C transfer (Section 5.2.9). */
    if (!ds3231_read_block(DS3231_REG_SECONDS, raw, 7U))
    {
        return false;
    }

    dt->seconds = DS3231_BcdToDec(raw[0] & 0x7FU);
    dt->minutes = DS3231_BcdToDec(raw[1] & 0x7FU);
    dt->hours   = ds3231_decode_hours24(raw[2]);
    dt->day     = raw[3] & 0x07U;
    dt->date    = DS3231_BcdToDec(raw[4] & 0x3FU);
    dt->month   = DS3231_BcdToDec(raw[5] & 0x1FU);
    dt->year    = DS3231_BcdToDec(raw[6]);

    if (dt->day < DS3231_DOW_SUN || dt->day > DS3231_DOW_SAT)
    {
        dt->day = DS3231_DOW_SUN;
    }

    return true;
}

bool DS3231_SetDateTime(const DS3231_DateTimeType *dt)
{
    uint8_t raw[7];
    uint16_t full_year;

    if (dt == NULL)
    {
        return false;
    }

    full_year = (uint16_t)(2000U + dt->year);
    if (!DS3231_IsValidTime(dt->hours, dt->minutes, dt->seconds) ||
        !DS3231_IsValidDateRange(dt->date, dt->month, full_year, dt->day) ||
        !DS3231_IsValidCalendarDate(dt->date, dt->month, full_year))
    {
        return false;
    }

    raw[0] = DS3231_DecToBcd(dt->seconds) & 0x7FU;
    raw[1] = DS3231_DecToBcd(dt->minutes) & 0x7FU;
    raw[2] = DS3231_DecToBcd(dt->hours) & 0x3FU; /* Bit 6 = 0 selects 24-hour mode */
    raw[3] = dt->day & 0x07U;
    raw[4] = DS3231_DecToBcd(dt->date) & 0x3FU;
    raw[5] = DS3231_DecToBcd(dt->month) & 0x1FU; /* Bit 7 (Century) = 0 for 2000-2099 */
    raw[6] = DS3231_DecToBcd(dt->year);

    return ds3231_write_block(DS3231_REG_SECONDS, raw, 7U);
}

bool DS3231_ReadTime(uint8_t *hours, uint8_t *minutes, uint8_t *seconds)
{
    uint8_t raw[3];

    if (hours == NULL || minutes == NULL || seconds == NULL)
    {
        return false;
    }

    if (!ds3231_read_block(DS3231_REG_SECONDS, raw, 3U))
    {
        return false;
    }

    *seconds = DS3231_BcdToDec(raw[0] & 0x7FU);
    *minutes = DS3231_BcdToDec(raw[1] & 0x7FU);
    *hours   = ds3231_decode_hours24(raw[2]);

    return true;
}

bool DS3231_SetTime(uint8_t hours, uint8_t minutes, uint8_t seconds)
{
    uint8_t raw[3];

    if (!DS3231_IsValidTime(hours, minutes, seconds))
    {
        return false;
    }

    raw[0] = DS3231_DecToBcd(seconds) & 0x7FU;
    raw[1] = DS3231_DecToBcd(minutes) & 0x7FU;
    raw[2] = DS3231_DecToBcd(hours) & 0x3FU; /* Bit 6 = 0 selects 24-hour mode */

    return ds3231_write_block(DS3231_REG_SECONDS, raw, 3U);
}

bool DS3231_ReadDate(uint8_t *date, uint8_t *month, uint8_t *year, uint8_t *day)
{
    uint8_t raw[4];

    if (date == NULL || month == NULL || year == NULL || day == NULL)
    {
        return false;
    }

    if (!ds3231_read_block(DS3231_REG_DAY, raw, 4U))
    {
        return false;
    }

    *day   = raw[0] & 0x07U;
    *date  = DS3231_BcdToDec(raw[1] & 0x3FU);
    *month = DS3231_BcdToDec(raw[2] & 0x1FU);
    *year  = DS3231_BcdToDec(raw[3]);

    if (*day < DS3231_DOW_SUN || *day > DS3231_DOW_SAT)
    {
        *day = DS3231_DOW_SUN;
    }

    return true;
}

bool DS3231_SetDate(uint8_t date, uint8_t month, uint8_t year, uint8_t day)
{
    uint8_t raw[4];
    uint16_t full_year = (uint16_t)(2000U + year);

    if (year > 99U ||
        !DS3231_IsValidDateRange(date, month, full_year, day) ||
        !DS3231_IsValidCalendarDate(date, month, full_year))
    {
        return false;
    }

    raw[0] = day & 0x07U;
    raw[1] = DS3231_DecToBcd(date) & 0x3FU;
    raw[2] = DS3231_DecToBcd(month) & 0x1FU;
    raw[3] = DS3231_DecToBcd(year);

    return ds3231_write_block(DS3231_REG_DAY, raw, 4U);
}

bool DS3231_ReadTemperature(DS3231_TempType *temp)
{
    uint8_t raw[2];
    int16_t q4_temp;
    uint16_t abs_q4;

    if (temp == NULL)
    {
        return false;
    }

    if (!ds3231_read_block(DS3231_REG_TEMP_MSB, raw, 2U))
    {
        return false;
    }

    /* Combine 8-bit signed MSB and upper 2 bits of LSB into signed quarter-degrees. */
    q4_temp = (int16_t)(((int16_t)((int8_t)raw[0]) * 4) + (int16_t)(raw[1] >> 6U));
    if (q4_temp < 0)
    {
        temp->negative = true;
        abs_q4 = (uint16_t)(-q4_temp);
        temp->integer = (int8_t)(-(int16_t)(abs_q4 >> 2U));
    }
    else
    {
        temp->negative = false;
        abs_q4 = (uint16_t)q4_temp;
        temp->integer = (int8_t)(abs_q4 >> 2U);
    }
    temp->frac_hundredths = (uint8_t)((abs_q4 & 0x03U) * 25U);

    return true;
}

bool DS3231_SetAlarm1(const DS3231_AlarmType *alarm, bool enable_interrupt)
{
    uint8_t raw[4];
    uint8_t ctrl;
    uint8_t stat;

    if (alarm == NULL || !DS3231_IsValidTime(alarm->hours, alarm->minutes, alarm->seconds))
    {
        return false;
    }

    raw[0] = (uint8_t)(DS3231_DecToBcd(alarm->seconds) | ((alarm->alarm_type & 0x01U) ? 0x80U : 0x00U));
    raw[1] = (uint8_t)(DS3231_DecToBcd(alarm->minutes) | ((alarm->alarm_type & 0x02U) ? 0x80U : 0x00U));
    raw[2] = (uint8_t)(DS3231_DecToBcd(alarm->hours)   | ((alarm->alarm_type & 0x04U) ? 0x80U : 0x00U));
    raw[3] = (uint8_t)(DS3231_DecToBcd(alarm->day_date) |
                       ((alarm->alarm_type & 0x08U) ? 0x80U : 0x00U) |
                       ((alarm->alarm_type & 0x10U) ? 0x40U : 0x00U));

    if (!ds3231_write_block(DS3231_REG_ALARM1_SECONDS, raw, 4U))
    {
        return false;
    }

    if (!ds3231_read_reg(DS3231_REG_STATUS, &stat))
    {
        return false;
    }
    stat &= (uint8_t)~DS3231_STAT_A1F;
    if (!ds3231_write_reg(DS3231_REG_STATUS, stat))
    {
        return false;
    }

    if (!ds3231_read_reg(DS3231_REG_CONTROL, &ctrl))
    {
        return false;
    }
    ctrl |= DS3231_CTRL_INTCN;
    if (enable_interrupt)
    {
        ctrl |= DS3231_CTRL_A1IE;
    }
    else
    {
        ctrl &= (uint8_t)~DS3231_CTRL_A1IE;
    }

    return ds3231_write_reg(DS3231_REG_CONTROL, ctrl);
}

bool DS3231_DisableAlarm1(void)
{
    uint8_t ctrl;
    if (!ds3231_read_reg(DS3231_REG_CONTROL, &ctrl))
    {
        return false;
    }
    ctrl &= (uint8_t)~DS3231_CTRL_A1IE;
    return ds3231_write_reg(DS3231_REG_CONTROL, ctrl);
}

bool DS3231_CheckAndClearAlarm1(bool *triggered)
{
    uint8_t stat;

    if (triggered == NULL)
    {
        return false;
    }
    *triggered = false;

    if (!ds3231_read_reg(DS3231_REG_STATUS, &stat))
    {
        return false;
    }

    if ((stat & DS3231_STAT_A1F) != 0U)
    {
        *triggered = true;
        stat &= (uint8_t)~DS3231_STAT_A1F;
        return ds3231_write_reg(DS3231_REG_STATUS, stat);
    }

    return true;
}

