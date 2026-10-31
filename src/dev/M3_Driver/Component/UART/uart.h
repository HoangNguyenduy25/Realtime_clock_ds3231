#ifndef UART_H
#define UART_H

#include <stdint.h>
#include <stdbool.h>
#include <avr/io.h>

#include "../../../M4_Middleware/FIFO/fifo.h"

typedef enum
{
    UART_NUMBER_0 = 0,
    UART_NUMBER_1 = 1,
} uart_number_t;

typedef enum
{
    UART_STATUS_STOPPED = 0,
    UART_STATUS_READY = 1,
} uart_status_t;

struct uart_dev;
typedef struct uart_dev uart_dev_t;

/**
 * Called from the RX ISR after a byte has been queued
 * Keep it short and nonblocking; the callback can drain bytes with uart_read()
 */
typedef void (*uart_rx_callback_t)(uart_dev_t *dev, void *user);
typedef void (*uart_tx_callback_t)(uart_dev_t *dev);

/**
 * The BSP owns number, baudrate, status, FIFO pointers, and tx_callback
 * Callers register the optional RX callback before global interrupts
 */
struct uart_dev
{
    uart_number_t number; /* UART number (0 or 1) */
    uint32_t baudrate; /* Baud rate in bits per second */
    volatile uart_status_t status; /* Current status of the UART */
    FIFO_t *rx_fifo; /* Pointer to the RX FIFO buffer */
    FIFO_t *tx_fifo; /* Pointer to the TX FIFO buffer */
    uart_rx_callback_t rx_callback; /* Callback function for RX events */
    void *callback_user; /* User data for the RX callback */
    uart_tx_callback_t tx_callback; /* Callback function for TX events */
};

/**
 * Nonblocking, false means unavailable, TX full, RX empty
 */
bool uart_write(uart_dev_t *dev, uint8_t value);
bool uart_read(uart_dev_t *dev, uint8_t *value);

/**
 * Configure before enabling global interrupts
 */
void uart_set_rx_callback(uart_dev_t *dev, uart_rx_callback_t callback, void *user);

/**
 * Compile-time platform hooks implemented by the selected BSP
 */
bool uart_platform_can_write(const uart_dev_t *dev);
void uart_platform_start_tx(uart_dev_t *dev);

#endif /* UART_H */
