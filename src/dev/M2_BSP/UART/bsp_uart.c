#include "bsp_uart.h"

#include <avr/io.h>
#include <avr/interrupt.h>
#include <stddef.h>
#include <util/atomic.h>

#include "../board.h"
#include "../../M3_Driver/Component/UART0/uart0.h"
#include "../../M3_Driver/Component/UART/uart.h"
#include "../../M4_Middleware/FIFO/fifo.h"

#define BSP_UART0_RX_FIFO_SIZE 64U
#define BSP_UART0_TX_FIFO_SIZE 256U

static uint8_t s_uart0_rx_buf[BSP_UART0_RX_FIFO_SIZE];
static uint8_t s_uart0_tx_buf[BSP_UART0_TX_FIFO_SIZE];
static FIFO_t  s_uart0_rx_fifo;
static FIFO_t  s_uart0_tx_fifo;

static uart_dev_t s_uart0_dev = {
    .number        = UART_NUMBER_0,
    .baudrate      = UART_XTP_BAUD,
    .status        = UART_STATUS_STOPPED,
    .rx_fifo       = &s_uart0_rx_fifo,
    .tx_fifo       = &s_uart0_tx_fifo,
    .rx_callback   = NULL,
    .callback_user = NULL,
    .tx_callback   = UART0_TxReadyCallback
};

/* Platform Hooks Required by M3_Driver/Component/UART/uart.c */
bool uart_platform_can_write(const uart_dev_t *dev)
{
    if (dev == NULL || dev->status != UART_STATUS_READY)
    {
        return false;
    }
    return (dev->number == UART_NUMBER_0);
}

void uart_platform_start_tx(uart_dev_t *dev)
{
    if (dev != NULL && dev->number == UART_NUMBER_0 && dev->status == UART_STATUS_READY)
    {
        UART0_TxStart();
    }
}

/* BSP UART Public Functions                                                  */
bool BSP_UART0_Init(uint32_t cpu_hz, uint32_t baudrate)
{
    if (cpu_hz == 0UL || baudrate == 0UL)
    {
        return false;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        s_uart0_dev.status = UART_STATUS_STOPPED;

        /* PD0 (RXD0) as input with pull-up; PD1 (TXD0) as output idle high */
        UART_XTP_RX_DDR  &= (uint8_t)~_BV(UART_XTP_RX_PIN);
        UART_XTP_RX_PORT |= _BV(UART_XTP_RX_PIN);
        UART_XTP_TX_PORT |= _BV(UART_XTP_TX_PIN);
        UART_XTP_TX_DDR  |= _BV(UART_XTP_TX_PIN);
    }

    if (!FIFO_Init(&s_uart0_rx_fifo, s_uart0_rx_buf, BSP_UART0_RX_FIFO_SIZE) ||
        !FIFO_Init(&s_uart0_tx_fifo, s_uart0_tx_buf, BSP_UART0_TX_FIFO_SIZE))
    {
        return false;
    }

    s_uart0_dev.number      = UART_NUMBER_0;
    s_uart0_dev.baudrate    = baudrate;
    s_uart0_dev.rx_fifo     = &s_uart0_rx_fifo;
    s_uart0_dev.tx_fifo     = &s_uart0_tx_fifo;
    s_uart0_dev.tx_callback = UART0_TxReadyCallback;

    if (!UART0_Init(cpu_hz, baudrate, &s_uart0_rx_fifo, &s_uart0_tx_fifo))
    {
        return false;
    }

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        s_uart0_dev.status = UART_STATUS_READY;
    }

    return true;
}

void BSP_UART0_Deinit(void)
{
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        s_uart0_dev.status = UART_STATUS_STOPPED;
        UART0_Deinit();
        FIFO_Clear(&s_uart0_rx_fifo);
        FIFO_Clear(&s_uart0_tx_fifo);
    }
}

uart_dev_t *BSP_UART0_Device(void)
{
    return &s_uart0_dev;
}

bool BSP_UART_InitBoard(void)
{
    return BSP_UART0_Init(F_CPU, UART_XTP_BAUD);
}

bool BSP_UART_Init(uint32_t cpu_hz, uint32_t pc104_baud, uint32_t console_baud)
{
    (void)console_baud;
    return BSP_UART0_Init(cpu_hz, pc104_baud);
}

void BSP_UART_Deinit(void)
{
    BSP_UART0_Deinit();
}

bool BSP_UART_Write(uart_number_t number, uint8_t value)
{
    if (number != UART_NUMBER_0)
    {
        return false;
    }
    return uart_write(&s_uart0_dev, value);
}

bool BSP_UART_Read(uart_number_t number, uint8_t *value)
{
    if (number != UART_NUMBER_0)
    {
        return false;
    }
    return uart_read(&s_uart0_dev, value);
}

/* Hardware USART0 Interrupt Service Routines */
#if defined(USART0_RX_vect)
ISR(USART0_RX_vect)
#elif defined(USART_RXC_vect)
ISR(USART_RXC_vect)
#endif
{
    if (UART0_OnRxInterrupt())
    {
        if (s_uart0_dev.rx_callback != NULL)
        {
            s_uart0_dev.rx_callback(&s_uart0_dev, s_uart0_dev.callback_user);
        }
    }
}

#if defined(USART0_UDRE_vect)
ISR(USART0_UDRE_vect)
#elif defined(USART_UDRE_vect)
ISR(USART_UDRE_vect)
#endif
{
    if (s_uart0_dev.tx_callback != NULL)
    {
        s_uart0_dev.tx_callback(&s_uart0_dev);
    }
    else
    {
        UART0_TxReadyCallback(&s_uart0_dev);
    }
}
