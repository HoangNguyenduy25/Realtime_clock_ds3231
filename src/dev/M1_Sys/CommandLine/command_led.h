#ifndef COMMAND_LED_H_
#define COMMAND_LED_H_

#include "command.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Handler for "LED" control commands:
 * - LED,ON          -> +OK,LED,ON
 * - LED,OFF         -> +OK,LED,OFF
 * - LED,BLINK,<T>   -> +OK,LED,BLINK,<T>  (T in 10..5000 ms)
 *
 * Error codes:
 * - E02: WRONG PARAM COUNT (missing or extra parameters)
 * - E03: PARAM OUT OF RANGE (keyword not ON/OFF/BLINK, or T outside 10..5000)
 * - E04: BAD FORMAT (T is not a valid integer)
 */
int Cmd_Led(CommandContext *context, int argc, char *argv[]);

/**
 * Handler for "LED?" query command:
 * - LED? -> +OK,LED,ON | +OK,LED,OFF | +OK,LED,BLINK,<T>
 *
 * Error codes:
 * - E02: WRONG PARAM COUNT (if any parameter is supplied)
 */
int Cmd_LedQuery(CommandContext *context, int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_LED_H_ */

