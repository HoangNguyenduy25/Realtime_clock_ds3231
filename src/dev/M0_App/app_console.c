#include "app_console.h"

#include <stddef.h>
#include <util/atomic.h>

#include "../M1_Sys/CommandLine/cmdline.h"
#include "../M1_Sys/CommandLine/command.h"
#include "../M2_BSP/UART/bsp_uart.h"
#include "../M4_Middleware/FIFO/fifo.h"

/**
 * Co-routine states for non-blocking UART Console processing.
 */
typedef enum
{
    APP_CONSOLE_CO_INIT = 0,
    APP_CONSOLE_CO_POLL_RX,
    APP_CONSOLE_CO_FLUSH_TX
} AppConsole_CoState_t;

static AppConsole_CoState_t s_console_co_state = APP_CONSOLE_CO_INIT;
static CommandContext       s_console_ctx;
static uint8_t              s_console_tx_storage[APP_CONSOLE_TX_FIFO_SIZE];
static FIFO_t               s_console_tx_fifo;
static bool                 s_console_initialized = false;


/* Internal Non-Blocking Output SinK */

/**
 * Flush up to max_bytes from the console staging FIFO into the BSP UART0 TX FIFO.
 */
static void app_console_flush_staging_tx(uint8_t max_bytes)
{
    uint8_t count = 0U;
    uint8_t byte  = 0U;

    while (count < max_bytes && FIFO_Count(&s_console_tx_fifo) > 0U)
    {
        /* Peek front byte at tail; only pop after BSP_UART_Write succeeds to preserve order */
        byte = s_console_tx_fifo.buffer[s_console_tx_fifo.tail];

        if (!BSP_UART_Write(UART_NUMBER_0, byte))
        {
            /* Hardware UART0 TX FIFO is temporarily full; retry on next co-routine tick */
            break;
        }

        (void)FIFO_Pop(&s_console_tx_fifo, &byte);
        count++;
    }

    s_console_ctx.busy = (FIFO_Count(&s_console_tx_fifo) > 0U);
}

/**
 * Callback registered with Command_Init(): invoked whenever Command_WriteRaw()
 * outputs response text ("+OK...\r\n" or "-ERR...\r\n").
 */
static void app_console_output_callback(const char *text, void *user_data)
{
    uint16_t i = 0U;
    (void)user_data;

    if (text == NULL)
    {
        return;
    }

    while (text[i] != '\0')
    {
        uint8_t b = (uint8_t)text[i];

        /* Fast path: if staging FIFO is empty, push directly to BSP UART0 TX FIFO */
        if (FIFO_Count(&s_console_tx_fifo) == 0U)
        {
            if (!BSP_UART_Write(UART_NUMBER_0, b))
            {
                (void)FIFO_Push(&s_console_tx_fifo, b);
            }
        }
        else
        {
            (void)FIFO_Push(&s_console_tx_fifo, b);
        }
        i++;
    }

    s_console_ctx.busy = (FIFO_Count(&s_console_tx_fifo) > 0U);
}

/* Public APIs                                                                */
bool AppConsole_Init(void)
{
    if (!FIFO_Init(&s_console_tx_fifo, s_console_tx_storage, APP_CONSOLE_TX_FIFO_SIZE))
    {
        return false;
    }

    Command_Init(&s_console_ctx, app_console_output_callback, &s_console_tx_fifo);

    if (!BSP_UART_InitBoard())
    {
        return false;
    }

    s_console_initialized = true;
    s_console_co_state    = APP_CONSOLE_CO_POLL_RX;
    return true;
}

bool AppConsole_WriteByte(uint8_t byte)
{
    if (!s_console_initialized)
    {
        return false;
    }

    if (FIFO_Count(&s_console_tx_fifo) == 0U)
    {
        if (BSP_UART_Write(UART_NUMBER_0, byte))
        {
            return true;
        }
    }

    if (!FIFO_Push(&s_console_tx_fifo, byte))
    {
        return false;
    }

    s_console_ctx.busy = true;
    return true;
}

uint16_t AppConsole_WriteString(const char *str)
{
    uint16_t written = 0U;

    if (str == NULL || !s_console_initialized)
    {
        return 0U;
    }

    while (str[written] != '\0')
    {
        if (!AppConsole_WriteByte((uint8_t)str[written]))
        {
            break;
        }
        written++;
    }

    return written;
}

bool AppConsole_WriteLine(const char *line)
{
    if (line != NULL)
    {
        (void)AppConsole_WriteString(line);
    }
    return (AppConsole_WriteString("\r\n") == 2U);
}

uint8_t AppConsole_ExecuteLine(const char *cmd_line)
{
    uint8_t status;

    if (!s_console_initialized)
    {
        (void)AppConsole_Init();
    }

    status = Command_ExecuteLine(&s_console_ctx, cmd_line);
    if (FIFO_Count(&s_console_tx_fifo) > 0U)
    {
        s_console_co_state = APP_CONSOLE_CO_FLUSH_TX;
    }
    return status;
}

bool AppConsole_IsBusy(void)
{
    return s_console_ctx.busy;
}

CommandContext *AppConsole_GetContext(void)
{
    return &s_console_ctx;
}

void AppConsole_Task(void)
{
    switch (s_console_co_state)
    {
        case APP_CONSOLE_CO_INIT:
            (void)AppConsole_Init();
            return; /* Co-routine yield */

        case APP_CONSOLE_CO_FLUSH_TX:
            app_console_flush_staging_tx(APP_CONSOLE_TX_BUDGET_PER_TICK);
            if (!s_console_ctx.busy)
            {
                s_console_co_state = APP_CONSOLE_CO_POLL_RX;
            }
            return; /* Co-routine yield */

        case APP_CONSOLE_CO_POLL_RX:
        default:
        {
            uint8_t processed = 0U;
            uint8_t rx_byte   = 0U;

            /* If any staged TX bytes remain from a previous tick, drain a slice first */
            if (FIFO_Count(&s_console_tx_fifo) > 0U)
            {
                app_console_flush_staging_tx(APP_CONSOLE_TX_BUDGET_PER_TICK);
            }

            /* Drain up to APP_CONSOLE_RX_BUDGET_PER_TICK bytes from the ISR RX ring buffer */
            while (processed < APP_CONSOLE_RX_BUDGET_PER_TICK &&
                   BSP_UART_Read(UART_NUMBER_0, &rx_byte))
            {
                processed++;

                if (Command_FeedChar(&s_console_ctx, (char)rx_byte))
                {
                    /* Complete command line was parsed & executed; yield after starting TX */
                    if (s_console_ctx.busy)
                    {
                        s_console_co_state = APP_CONSOLE_CO_FLUSH_TX;
                    }
                    return; /* Co-routine yield */
                }
            }

            return; /* Co-routine yield */
        }
    }
}

void AppConsole_Run(void)
{
    AppConsole_Task();
}
