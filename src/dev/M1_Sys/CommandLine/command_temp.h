#ifndef COMMAND_TEMP_H_
#define COMMAND_TEMP_H_

#include "command.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Handler for "TEMP?" query command:
 * - TEMP? -> +OK,TEMP,<val>,C   (e.g. "+OK,TEMP,28.25,C")
 * - Errors: E02 (extra params), E05 (DS3231 read error)
 */
int Cmd_TempQuery(CommandContext *context, int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_TEMP_H_ */

