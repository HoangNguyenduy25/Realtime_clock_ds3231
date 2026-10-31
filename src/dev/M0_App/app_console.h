#ifndef APP_CONSOLE_H_
#define APP_CONSOLE_H_

#include <stdbool.h>
#include <stdint.h>

#include "../M1_Sys/CommandLine/cmdline.h"
#include "../M1_Sys/CommandLine/command.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Per-tick non-blocking byte budgets for the Console Co-routine (NFR-01 <= 5 ms, NFR-03 <= 100 ms).
 */
#define APP_CONSOLE_TX_FIFO_SIZE       32U
#define APP_CONSOLE_RX_BUDGET_PER_TICK 16U
#define APP_CONSOLE_TX_BUDGET_PER_TICK 32U

/**
 * Initialize UART0 (9600 8N1 via BSP_UART_InitBoard), the console TX staging FIFO,
 * and the CommandContext state machine.
 */
bool AppConsole_Init(void);

/**
 * Cooperative RTOS Co-routine task for UART command reception, parsing, and response flushing.
 * Register in the scheduler task table (recommended period: 1..5 ms).
 * Satisfies FR-03, NFR-01, NFR-02, and NFR-03.
 */
void AppConsole_Task(void);

/**
 * Sequential console runner alias (referenced by cmdline.c).
 */
void AppConsole_Run(void);

/**
 * Non-blocking console output helpers (queue bytes to UART0 TX FIFO / staging FIFO).
 */
bool     AppConsole_WriteByte(uint8_t byte);
uint16_t AppConsole_WriteString(const char *str);
bool     AppConsole_WriteLine(const char *line);

/**
 * Execute a null-terminated command line directly and queue its response to UART0.
 *
 * @return CMDLINE_OK (0) on "+OK" response, or non-zero error code on "-ERR".
 */
uint8_t AppConsole_ExecuteLine(const char *cmd_line);

/**
 * Return true if the console is still flushing a previous multi-line response.
 */
bool AppConsole_IsBusy(void);

/**
 * Access the underlying CommandContext instance.
 */
CommandContext *AppConsole_GetContext(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_CONSOLE_H_ */

