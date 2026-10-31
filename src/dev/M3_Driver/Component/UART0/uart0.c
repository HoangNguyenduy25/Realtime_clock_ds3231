#include "uart0.h"

#include <avr/io.h>
#include <stddef.h>
#include <util/atomic.h>

static FIFO_t *s_rx_fifo;
static volatile uint16_t s_rx_dropped;
static volatile uint16_t s_rx_errors;

bool UART0_Init(uint32_t cpu_hz, uint32_t baud, FIFO_t *rx_fifo, FIFO_t *tx_fifo)
{
    uint32_t divisor;
    uint32_t ubrr;

    if (rx_fifo == NULL || tx_fifo == NULL || rx_fifo == tx_fifo || rx_fifo->buffer == NULL || tx_fifo->buffer == NULL || rx_fifo->capacity == 0u || 
        tx_fifo->capacity == 0u || baud == 0u || baud > 1000000UL || cpu_hz == 0u )
    {
        return false;
    }

    divisor = baud * 8UL;
    ubrr = (cpu_hz + divisor / 2UL) / divisor;
    if (ubrr == 0u || ubrr > 4096u)
    {
        return false;
    }
    ubrr--;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        UCSR0B = 0u; // Disable USART0 while configuring
        s_rx_fifo = rx_fifo;
        s_rx_dropped = 0u;
        s_rx_errors = 0u;
        UBRR0H = (uint8_t)(ubrr >> 8u);
        UBRR0L = (uint8_t)ubrr;
        UCSR0A = _BV(U2X0); // Double speed
        UCSR0C = _BV(UCSZ01) | _BV(UCSZ00); // 8N1
        UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0); // Enable USART0 with interrupts
        if (FIFO_Count(tx_fifo) > 0u)
        {
            UCSR0B |= _BV(UDRIE0); // Enable TX interrupt if there is data to send
        }
    }

    return true;
}

void UART0_Deinit(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        UCSR0B = 0u; // Disable USART0
        s_rx_fifo = NULL;
    }
}

void UART0_TxStart(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if ((UCSR0B & _BV(TXEN0)) != 0u)
        {
            UCSR0B |= _BV(UDRIE0); // Enable TX interrupt if there is data to send
        }
    }
}

bool UART0_Read(uint8_t *value)
{
    bool read = false;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        if (s_rx_fifo != NULL)
        {
            read = FIFO_Pop(s_rx_fifo, value);
        }
    }
    return read;
}

uint16_t UART0_RxDropped(void)
{
    uint16_t value;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        value = s_rx_dropped;
    }
    return value;
}

uint16_t UART0_RxErrors(void)
{
    uint16_t value;

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        value = s_rx_errors;
    }
    return value;
}
bool UART0_OnRxInterrupt(void)
{
    uint8_t status = UCSR0A;
    uint8_t value = UDR0;

    if ((status & (_BV(FE0) | _BV(DOR0) | _BV(UPE0))) != 0u)
    {
        s_rx_errors++;
        return false;
    }
    else if (s_rx_fifo == NULL || !FIFO_Push(s_rx_fifo, value))
    {
        s_rx_dropped++;
        return false;
    }
    return true;
}

void UART0_TxReadyCallback(uart_dev_t *dev)
{
    uint8_t value;
    if (dev != NULL && dev->tx_fifo != NULL && FIFO_Pop(dev->tx_fifo, &value))
    {
        UDR0 = value;
    }
    else
    {
        UCSR0B &= (uint8_t)~_BV(UDRIE0); // Disable TX interrupt if there is no more data to send
    }
}