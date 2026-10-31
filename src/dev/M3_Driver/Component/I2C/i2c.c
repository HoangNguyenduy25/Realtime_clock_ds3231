#include "i2c.h"
#include <avr/io.h>
#include <util/twi.h>

#define I2C_WAIT_LIMIT 10000u
static bool s_timed_out;

static bool I2C_AVR_Wait(void)
{
    uint16_t remaining = I2C_WAIT_LIMIT;
    while((TWCR & _BV(TWINT)) == 0U && remaining-- != 0U) {
        /* Wait for TWINT to be set or timeout. */
    }
    if((TWCR & _BV(TWINT)) != 0U)
    {
        return true; /* TWINT set, operation completed. */
    }
    s_timed_out = true;
    return false; /* Timeout occurred. */
}

bool I2C_AVR_Init(uint32_t cpu_hz, uint32_t bus_hz)
{
    uint32_t divisor;
    if(bus_hz == 0U || cpu_hz / bus_hz <16U) {
        return false; /* Invalid bus frequency. */
    }
    divisor = (cpu_hz / bus_hz - 16U) / 2U;
    if(divisor > 255U) {
        return false; /* Divisor too large. */
    }

    TWSR &= (uint8_t)~0x03U; /* Prescaler =1 */
    TWBR = (uint8_t)divisor;
    TWCR = _BV(TWEN);
    return true;
}

bool I2C_AVR_Start(uint8_t address_rw)
{
    uint8_t status;
    TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN); /* Send START condition. */
    if(!I2C_AVR_Wait()) {
        return false; /* Timeout occurred. */
    }
    status = TW_STATUS;
    if(status != TW_START && status != TW_REP_START) {
        return false; /* Failed to send START condition. */
    }   
    TWDR = address_rw; /* Load address and R/W bit. */
    TWCR = _BV(TWINT) | _BV(TWEN); /* Start transmission. */
    if(!I2C_AVR_Wait()) {
        return false; /* Timeout occurred. */
    }
    status = TW_STATUS;
    return status == TW_MT_SLA_ACK || status == TW_MR_SLA_ACK; /* Check for ACK. */
}

bool I2C_AVR_Write(uint8_t value)
{
    TWDR = value; /* Load data to be transmitted. */
    TWCR = _BV(TWINT) | _BV(TWEN); /* Start transmission. */
    return I2C_AVR_Wait() && TW_STATUS == TW_MT_DATA_ACK; /* Wait for ACK. */
}

bool I2C_AVR_Read(uint8_t *value, bool ack)
{
    if (value == 0) 
    {
        return false; /* Null pointer check. */
    }
    TWCR = _BV(TWINT) | _BV(TWEN) | (ack ? _BV(TWEA) : 0u); /* Start reception. */
    if(!I2C_AVR_Wait() || TW_STATUS != (ack ? TW_MR_DATA_ACK : TW_MR_DATA_NACK)) {
        return false; /* Timeout or NACK received. */
    }
    *value = TWDR; /* Read received data. */
    return true;
}

void I2C_AVR_Stop(void)
{
    TWCR = _BV(TWINT) | _BV(TWEN) | _BV(TWSTO); /* Send STOP condition. */
    
}

void I2C_AVR_ClearTimeout(void)
{
    s_timed_out = false;
}

bool I2C_AVR_TimedOut(void)
{
    return s_timed_out;
}

void I2C_AVR_Recover(void)
{
    TWCR = 0U; /* Disable TWI. */
    TWCR = _BV(TWEN); /* Re-enable TWI. */
}