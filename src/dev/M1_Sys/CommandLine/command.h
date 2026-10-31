#ifndef COMMAND_H_
#define COMMAND_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Protocol framing constants of the specification:
 * - Maximum command line length: 48 bytes including CR/LF terminator (E06 if exceeded).
 * - Multi-line HELP output requires ~210 bytes, so response buffer is 256 bytes.
 */
#define COMMAND_MAX_LINE_LENGTH      48U
#define COMMAND_WORK_BUFFER_SIZE     96U
#define COMMAND_RESPONSE_MAX_LENGTH  64U

/**
 * Fixed firmware identifier returned by the ID? command.
 */
#define COMMAND_FIRMWARE_ID          "ATMEGA324-RTC,V1.0"

/**
 * Placeholder byte used internally when normalizing empty comma-separated fields
 * before invoking CmdLineProcess().
 */
#define COMMAND_EMPTY_ARG_CHAR       ((char)0x7F)
#define COMMAND_SPACE_GUARD_CHAR     ((char)0x1F)

/**
 * Protocol error codes defined
 */
typedef enum
{
    CMD_ERR_NONE = 0,
    CMD_ERR_E01_UNKNOWN_CMD,      /* -ERR,E01,UNKNOWN COMMAND */
    CMD_ERR_E02_WRONG_PARAM_CNT,  /* -ERR,E02,WRONG PARAM COUNT */
    CMD_ERR_E03_OUT_OF_RANGE,     /* -ERR,E03,PARAM OUT OF RANGE */
    CMD_ERR_E04_BAD_FORMAT,       /* -ERR,E04,BAD FORMAT */
    CMD_ERR_E05_RTC_ERROR,        /* -ERR,E05,RTC ERROR */
    CMD_ERR_E06_LINE_TOO_LONG,    /* -ERR,E06,LINE TOO LONG */
    CMD_ERR_E07_INVALID_DATE,     /* -ERR,E07,INVALID DATE */
    CMD_ERR_E08_BUSY              /* -ERR,E08,BUSY */
} Command_ErrorCode_t;

/**
 * Optional callback invoked whenever response text is appended to CommandContext.
 */
typedef void (*Command_OutputCallback_t)(const char *text, void *user_data);

/**
 * Command execution & non-blocking line collector context.
 * Defined before including cmdline.h so pfnCmdLine has the full type definition.
 */
typedef struct CommandContext
{
    /* Non-blocking RX line assembly state */
    char                     rx_buf[COMMAND_MAX_LINE_LENGTH + 1U];
    uint8_t                  rx_index;
    bool                     rx_overflow;
    bool                     last_was_cr;
    bool                     busy;

    /* Formatted response buffer */
    char                     response[COMMAND_RESPONSE_MAX_LENGTH];
    uint16_t                 response_len;
    bool                     response_ready;
    Command_ErrorCode_t      last_error;

    /* Optional immediate output sink */
    Command_OutputCallback_t output_cb;
    void                    *user_data;
} CommandContext;


/* Context Lifecycle & Non-Blocking Line Processing APIs */

/**
 * Initialize a CommandContext instance and optionally attach an output callback.
 */
void Command_Init(CommandContext *context,
                  Command_OutputCallback_t output_cb,
                  void *user_data);

/**
 * Clear the response buffer inside the given context prior to executing a new command.
 */
void Command_ClearResponse(CommandContext *context);

/**
 * Feed one received UART character into the non-blocking line collector.
 * - Accepts both CR+LF ("\r\n") and LF ("\n") line endings.
 * - Enforces the 48-character maximum line length including terminator.
 * - Automatically executes the command and populates context->response when a line completes.
 *
 * @return true if a complete command line was processed and a response was generated.
 */
bool Command_FeedChar(CommandContext *context, char rx_char);

/**
 * Execute a complete command line string (with or without trailing CR/LF).
 * Normalizes comma delimiters and case-insensitive command names, dispatches via CmdLineProcess()
 * @return CMDLINE_OK (0) on "+OK" response, or non-zero on "-ERR" response.
 */
uint8_t Command_ExecuteLine(CommandContext *context, const char *raw_line);

/* Response Formatting Helpers for Command Handlers */

/**
 * Append raw string to the response buffer (and forward to output_cb if set).
 */
void Command_WriteRaw(CommandContext *context, const char *text);

/**
 * Append a single line followed by "\r\n" to the response buffer.
 */
void Command_WriteLine(CommandContext *context, const char *line);

/**
 * Format a standard single-line success response:
 * - If payload is NULL or empty: "+OK\r\n"
 * - Otherwise:                   "+OK,<payload>\r\n"
 */
int Command_ReplyOk(CommandContext *context, const char *payload);

/**
 * Format a standard single-line error response:
 * "-ERR,<E01..E08>,<short_description>\r\n"
 */
int Command_ReplyError(CommandContext *context, Command_ErrorCode_t err_code);

/* Argument Validation Utilities for Sub-Command Modules */

/**
 * Returns true if the given token is NULL, empty, or the empty-comma placeholder.
 */
bool Command_IsEmptyArg(const char *arg);

/**
 * Returns true if any argv[1..argc-1] is an empty-comma placeholder.
 */
bool Command_HasEmptyArg(int argc, char *argv[]);

/**
 * Case-insensitive ASCII string comparison (ignores space-guard characters).
 */
bool Command_StrEqualsIgnoreCase(const char *a, const char *b);

/**
 * Format an unsigned 16-bit integer into decimal ASCII digits.
 */
void Command_FormatU16(uint16_t value, char *out_buf);

/* Built-in System Command Handlers */

int Cmd_Ping(CommandContext *context, int argc, char *argv[]);
int Cmd_IdQuery(CommandContext *context, int argc, char *argv[]);
int Cmd_Help(CommandContext *context, int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

#endif /* COMMAND_H_ */

