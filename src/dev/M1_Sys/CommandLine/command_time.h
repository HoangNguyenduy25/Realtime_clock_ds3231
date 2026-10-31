#ifndef COMMAND_TIME_H_
#define COMMAND_TIME_H_

#include "command.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Handler for "TIME?" query command:
 * - TIME? -> +OK,TIME,<hh:mm:ss>
 * - Errors: E02 (extra params), E05 (DS3231 read error)
 */
int Cmd_TimeQuery(CommandContext *context, int argc, char *argv[]);

/**
 * Handler for "DATE?" query command:
 * - DATE? -> +OK,DATE,<dd/mm/yyyy>,<DOW>
 * - Errors: E02 (extra params), E05 (DS3231 read error)
 */
int Cmd_DateQuery(CommandContext *context, int argc, char *argv[]);

/**
 * Handler for "DT?" atomic date+time query command:
 * - DT? -> +OK,DT,<dd/mm/yyyy>,<hh:mm:ss>,<DOW>
 * - Errors: E02 (extra params), E05 (DS3231 read error)
 */
int Cmd_DateTimeQuery(CommandContext *context, int argc, char *argv[]);

/**
 * Handler for "TIME,<hh:mm:ss>" set command:
 * - TIME,<hh:mm:ss> -> +OK,TIME,<hh:mm:ss>
 * - Errors: E02, E03, E04, E05
 */
int Cmd_TimeSet(CommandContext *context, int argc, char *argv[]);

/**
 * Handler for "DATE,<dd/mm/yyyy>,<DOW>" set command:
 * - DATE,<dd/mm/yyyy>,<DOW> -> +OK,DATE,<dd/mm/yyyy>,<DOW>
 * - Errors: E02, E03, E04, E05, E07 (invalid calendar/leap date)
 */
int Cmd_DateSet(CommandContext *context, int argc, char *argv[]);

/**
 * Handler for "DT,<dd/mm/yyyy>,<hh:mm:ss>,<DOW>" atomic set command:
 * - DT,<dd/mm/yyyy>,<hh:mm:ss>,<DOW> -> +OK,DT,<dd/mm/yyyy>,<hh:mm:ss>,<DOW>
 * - Errors: E02, E03, E04, E05, E07
 */
int Cmd_DateTimeSet(CommandContext *context, int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_TIME_H_ */

