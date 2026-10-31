#include "uart.h"
#include <stddef.h>

bool uart_write(uart_dev_t *dev, uint8_t value)
{
    if (dev == NULL || dev->status != UART_STATUS_READY || dev->tx_fifo == NULL || !uart_platform_can_write(dev))
    {
        return false; /* Invalid device or not ready. */
    }
    if (!FIFO_Push(dev->tx_fifo, value))
    {
        return false; /* TX FIFO is full. */
    }
    uart_platform_start_tx(dev);
    return true;
}

bool uart_read(uart_dev_t *dev, uint8_t *value)
{
    if (dev == NULL || value == NULL || dev->status != UART_STATUS_READY || dev->rx_fifo == NULL)
    {
        return false; /* Invalid device or not ready. */
    }
    return FIFO_Pop(dev->rx_fifo, value); /* Attempt to read a byte from the RX FIFO. */
}

void uart_set_rx_callback(uart_dev_t *dev, uart_rx_callback_t callback, void *user)
{
    if (dev != NULL)
    {
        dev->rx_callback = callback; /* Set the RX callback function. */
        dev->callback_user = user; /* Set the user data for the RX callback. */
    }
}
