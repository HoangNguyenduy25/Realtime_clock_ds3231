#include "command.h"

#include <stddef.h>
#include <string.h>

#include "cmdline.h"
#include "command_led.h"
#include "command_temp.h"
#include "command_time.h"

/* PROGMEM Command Strings & Lookup Table */
static const char s_cmd_ping[]       CMD_TEXT_STORAGE = "PING";
static const char s_help_ping[]      CMD_TEXT_STORAGE = "PING";

static const char s_cmd_id_q[]       CMD_TEXT_STORAGE = "ID?";
static const char s_help_id_q[]      CMD_TEXT_STORAGE = "ID?";

static const char s_cmd_help[]       CMD_TEXT_STORAGE = "HELP";
static const char s_help_help[]      CMD_TEXT_STORAGE = "HELP";

static const char s_cmd_led[]        CMD_TEXT_STORAGE = "LED";
static const char s_help_led[]       CMD_TEXT_STORAGE = "LED,ON|OFF|BLINK,<10..5000>";

static const char s_cmd_led_q[]      CMD_TEXT_STORAGE = "LED?";
static const char s_help_led_q[]     CMD_TEXT_STORAGE = "LED?";

static const char s_cmd_time_q[]     CMD_TEXT_STORAGE = "TIME?";
static const char s_help_time_q[]    CMD_TEXT_STORAGE = "TIME?";

static const char s_cmd_date_q[]     CMD_TEXT_STORAGE = "DATE?";
static const char s_help_date_q[]    CMD_TEXT_STORAGE = "DATE?";

static const char s_cmd_dt_q[]       CMD_TEXT_STORAGE = "DT?";
static const char s_help_dt_q[]      CMD_TEXT_STORAGE = "DT?";

static const char s_cmd_time_set[]   CMD_TEXT_STORAGE = "TIME";
static const char s_help_time_set[]  CMD_TEXT_STORAGE = "TIME,<hh:mm:ss>";

static const char s_cmd_date_set[]   CMD_TEXT_STORAGE = "DATE";
static const char s_help_date_set[]  CMD_TEXT_STORAGE = "DATE,<dd/mm/yyyy>,<DOW>";

static const char s_cmd_dt_set[]     CMD_TEXT_STORAGE = "DT";
static const char s_help_dt_set[]    CMD_TEXT_STORAGE = "DT,<dd/mm/yyyy>,<hh:mm:ss>,<DOW>";

static const char s_cmd_temp_q[]     CMD_TEXT_STORAGE = "TEMP?";
static const char s_help_temp_q[]    CMD_TEXT_STORAGE = "TEMP?";

/**
 * Global command table required by cmdline.c (terminated with NULL entry).
 */
tCmdLineEntry g_psCmdTable[] =
{
    { s_cmd_ping,     Cmd_Ping,          s_help_ping     },
    { s_cmd_id_q,     Cmd_IdQuery,       s_help_id_q     },
    { s_cmd_help,     Cmd_Help,          s_help_help     },
    { s_cmd_led,      Cmd_Led,           s_help_led      },
    { s_cmd_led_q,    Cmd_LedQuery,      s_help_led_q    },
    { s_cmd_time_q,   Cmd_TimeQuery,     s_help_time_q   },
    { s_cmd_date_q,   Cmd_DateQuery,     s_help_date_q   },
    { s_cmd_dt_q,     Cmd_DateTimeQuery, s_help_dt_q     },
    { s_cmd_time_set, Cmd_TimeSet,       s_help_time_set },
    { s_cmd_date_set, Cmd_DateSet,       s_help_date_set },
    { s_cmd_dt_set,   Cmd_DateTimeSet,   s_help_dt_set   },
    { s_cmd_temp_q,   Cmd_TempQuery,     s_help_temp_q   },
    { NULL,           NULL,              NULL            }
};


/* Internal Helpers */
static char command_toupper_ascii(char c)
{
    if (c >= 'a' && c <= 'z')
    {
        return (char)(c - ('a' - 'A'));
    }
    return c;
}

static bool command_is_known_cmd(const char *cmd_name)
{
    const tCmdLineEntry *entry = &g_psCmdTable[0];

    if (cmd_name == NULL || cmd_name[0] == '\0')
    {
        return false;
    }

    while (entry->pcCmd != NULL)
    {
        if (CMD_TEXT_EQUAL(cmd_name, entry->pcCmd))
        {
            return true;
        }
        entry++;
    }
    return false;
}

/* Context Lifecycle & Formatting APIs */
void Command_Init(CommandContext *context,
                  Command_OutputCallback_t output_cb,
                  void *user_data)
{
    if (context == NULL)
    {
        return;
    }

    context->rx_buf[0]      = '\0';
    context->rx_index       = 0U;
    context->rx_overflow    = false;
    context->last_was_cr    = false;
    context->busy           = false;

    context->response[0]    = '\0';
    context->response_len   = 0U;
    context->response_ready = false;
    context->last_error     = CMD_ERR_NONE;

    context->output_cb      = output_cb;
    context->user_data      = user_data;
}

void Command_ClearResponse(CommandContext *context)
{
    if (context == NULL)
    {
        return;
    }

    context->response[0]    = '\0';
    context->response_len   = 0U;
    context->response_ready = false;
    context->last_error     = CMD_ERR_NONE;
}

void Command_WriteRaw(CommandContext *context, const char *text)
{
    uint16_t i = 0U;

    if (context == NULL || text == NULL)
    {
        return;
    }

    while (text[i] != '\0')
    {
        if ((context->response_len + 1U) < COMMAND_RESPONSE_MAX_LENGTH)
        {
            context->response[context->response_len] = text[i];
            context->response_len++;
            context->response[context->response_len] = '\0';
        }
        i++;
    }

    context->response_ready = true;

    if (context->output_cb != NULL)
    {
        context->output_cb(text, context->user_data);
    }
}

void Command_WriteLine(CommandContext *context, const char *line)
{
    if (line != NULL)
    {
        Command_WriteRaw(context, line);
    }
    Command_WriteRaw(context, "\r\n");
}

int Command_ReplyOk(CommandContext *context, const char *payload)
{
    if (context == NULL)
    {
        return CMDLINE_INVALID_ARG;
    }

    context->last_error = CMD_ERR_NONE;
    Command_WriteRaw(context, "+OK");
    if (payload != NULL && payload[0] != '\0')
    {
        Command_WriteRaw(context, ",");
        Command_WriteRaw(context, payload);
    }
    Command_WriteRaw(context, "\r\n");

    return CMDLINE_OK;
}

int Command_ReplyError(CommandContext *context, Command_ErrorCode_t err_code)
{
    const char *err_str;

    if (context == NULL)
    {
        return CMDLINE_INVALID_ARG;
    }

    context->last_error = err_code;

    switch (err_code)
    {
        case CMD_ERR_E01_UNKNOWN_CMD:
            err_str = "-ERR,E01,UNKNOWN COMMAND\r\n";
            break;
        case CMD_ERR_E02_WRONG_PARAM_CNT:
            err_str = "-ERR,E02,WRONG PARAM COUNT\r\n";
            break;
        case CMD_ERR_E03_OUT_OF_RANGE:
            err_str = "-ERR,E03,PARAM OUT OF RANGE\r\n";
            break;
        case CMD_ERR_E04_BAD_FORMAT:
            err_str = "-ERR,E04,BAD FORMAT\r\n";
            break;
        case CMD_ERR_E05_RTC_ERROR:
            err_str = "-ERR,E05,RTC ERROR\r\n";
            break;
        case CMD_ERR_E06_LINE_TOO_LONG:
            err_str = "-ERR,E06,LINE TOO LONG\r\n";
            break;
        case CMD_ERR_E07_INVALID_DATE:
            err_str = "-ERR,E07,INVALID DATE\r\n";
            break;
        case CMD_ERR_E08_BUSY:
            err_str = "-ERR,E08,BUSY\r\n";
            break;
        case CMD_ERR_NONE:
        default:
            err_str = "-ERR,E01,UNKNOWN COMMAND\r\n";
            break;
    }

    Command_WriteRaw(context, err_str);
    return (int)err_code;
}

/* Argument Validation Utilities */
bool Command_IsEmptyArg(const char *arg)
{
    if (arg == NULL || arg[0] == '\0')
    {
        return true;
    }
    if (arg[0] == COMMAND_EMPTY_ARG_CHAR && arg[1] == '\0')
    {
        return true;
    }
    return false;
}

bool Command_HasEmptyArg(int argc, char *argv[])
{
    int i;
    if (argv == NULL)
    {
        return true;
    }
    for (i = 1; i < argc; i++)
    {
        if (Command_IsEmptyArg(argv[i]))
        {
            return true;
        }
    }
    return false;
}

bool Command_StrEqualsIgnoreCase(const char *a, const char *b)
{
    uint8_t i = 0U;

    if (a == NULL || b == NULL)
    {
        return false;
    }

    while (a[i] != '\0' && b[i] != '\0')
    {
        if (command_toupper_ascii(a[i]) != command_toupper_ascii(b[i]))
        {
            return false;
        }
        i++;
    }

    return (a[i] == '\0' && b[i] == '\0');
}

void Command_FormatU16(uint16_t value, char *out_buf)
{
    char    rev[6];
    uint8_t len = 0U;
    uint8_t i;

    if (out_buf == NULL)
    {
        return;
    }

    if (value == 0U)
    {
        out_buf[0] = '0';
        out_buf[1] = '\0';
        return;
    }

    while (value > 0U && len < sizeof(rev))
    {
        rev[len++] = (char)('0' + (value % 10U));
        value /= 10U;
    }

    for (i = 0U; i < len; i++)
    {
        out_buf[i] = rev[len - 1U - i];
    }
    out_buf[len] = '\0';
}

/* Built-in System Commands: PING, ID?, HELP */
int Cmd_Ping(CommandContext *context, int argc, char *argv[])
{
    (void)argv;

    if (argc != 1)
    {
        return Command_ReplyError(context, CMD_ERR_E02_WRONG_PARAM_CNT);
    }

    return Command_ReplyOk(context, "PONG");
}

int Cmd_IdQuery(CommandContext *context, int argc, char *argv[])
{
    (void)argv;

    if (argc != 1)
    {
        return Command_ReplyError(context, CMD_ERR_E02_WRONG_PARAM_CNT);
    }

    return Command_ReplyOk(context, COMMAND_FIRMWARE_ID);
}

int Cmd_Help(CommandContext *context, int argc, char *argv[])
{
    (void)argv;

    if (argc != 1)
    {
        return Command_ReplyError(context, CMD_ERR_E02_WRONG_PARAM_CNT);
    }

    context->last_error = CMD_ERR_NONE;

    /* Exact multi-line output specified */
    Command_WriteLine(context, "PING");
    Command_WriteLine(context, "ID?");
    Command_WriteLine(context, "LED,ON|OFF");
    Command_WriteLine(context, "LED,BLINK,<10..5000>");
    Command_WriteLine(context, "LED?");
    Command_WriteLine(context, "TIME? | DATE? | DT? | TEMP?");
    Command_WriteLine(context, "TIME,<hh:mm:ss>");
    Command_WriteLine(context, "DATE,<dd/mm/yyyy>,<DOW>");
    Command_WriteLine(context, "DT,<dd/mm/yyyy>,<hh:mm:ss>,<DOW>");
    Command_WriteLine(context, "+OK,END");

    return CMDLINE_OK;
}

/* Line Parser & Pre-Processor for CmdLineProcess */
uint8_t Command_ExecuteLine(CommandContext *context, const char *raw_line)
{
    static char    work_buf[COMMAND_WORK_BUFFER_SIZE];
    char    cmd_token[COMMAND_MAX_LINE_LENGTH + 1U];
    uint8_t raw_total_len = 0U;
    uint8_t content_len   = 0U;
    uint8_t term_len      = 0U;
    uint8_t i             = 0U;
    uint8_t w_idx         = 0U;
    uint8_t c_idx         = 0U;
    bool    in_cmd_name   = true;
    uint8_t status;

    if (context == NULL)
    {
        return CMDLINE_INVALID_ARG;
    }

    Command_ClearResponse(context);

    if (context->busy)
    {
        (void)Command_ReplyError(context, CMD_ERR_E08_BUSY);
        return (uint8_t)CMD_ERR_E08_BUSY;
    }

    if (raw_line == NULL)
    {
        (void)Command_ReplyError(context, CMD_ERR_E01_UNKNOWN_CMD);
        return (uint8_t)CMD_ERR_E01_UNKNOWN_CMD;
    }

    /* Measure total line length and strip trailing CR / LF */
    while (raw_line[raw_total_len] != '\0')
    {
        raw_total_len++;
        if (raw_total_len > COMMAND_MAX_LINE_LENGTH)
        {
            (void)Command_ReplyError(context, CMD_ERR_E06_LINE_TOO_LONG);
            return (uint8_t)CMD_ERR_E06_LINE_TOO_LONG;
        }
    }

    content_len = raw_total_len;
    while (content_len > 0U &&
           (raw_line[content_len - 1U] == '\r' || raw_line[content_len - 1U] == '\n'))
    {
        content_len--;
        term_len++;
    }

    /* Maximum 48 characters INCLUDING line ending (at least 1 byte LF) */
    if ((term_len == 0U && content_len >= COMMAND_MAX_LINE_LENGTH) ||
        (term_len > 0U && raw_total_len > COMMAND_MAX_LINE_LENGTH))
    {
        (void)Command_ReplyError(context, CMD_ERR_E06_LINE_TOO_LONG);
        return (uint8_t)CMD_ERR_E06_LINE_TOO_LONG;
    }

    /* Empty line or leading comma -> E01 UNKNOWN COMMAND */
    if (content_len == 0U || raw_line[0] == ',')
    {
        (void)Command_ReplyError(context, CMD_ERR_E01_UNKNOWN_CMD);
        return (uint8_t)CMD_ERR_E01_UNKNOWN_CMD;
    }

    /**
     * Transform comma-separated frame "<CMD>[,<arg1>[,<arg2>...]]" into a space-separated
     * buffer for CmdLineProcess():
     * 1. Uppercase the command.
     * 2. Guard any raw ASCII spaces (' ') with COMMAND_SPACE_GUARD_CHAR (0x1F) so
     *    spaces are NOT treated as argument delimiters (Section 4.2: no extra spaces).
     * 3. Convert commas (',') into ' ' delimiters, inserting COMMAND_EMPTY_ARG_CHAR (0x7F)
     *    whenever a comma is followed by another comma or end-of-line so empty fields
     *    are preserved and flagged as E02 WRONG PARAM COUNT.
     */
    for (i = 0U; i < content_len; i++)
    {
        char c = raw_line[i];

        if (c == ',')
        {
            in_cmd_name = false;
            if ((w_idx + 3U) < sizeof(work_buf))
            {
                work_buf[w_idx++] = ' ';
                if ((i + 1U) >= content_len || raw_line[i + 1U] == ',')
                {
                    work_buf[w_idx++] = COMMAND_EMPTY_ARG_CHAR;
                }
            }
        }
        else
        {
            char mapped;
            if (c == ' ' || c < 0x20 || c > 0x7E)
            {
                mapped = COMMAND_SPACE_GUARD_CHAR;
            }
            else if (in_cmd_name)
            {
                mapped = command_toupper_ascii(c);
            }
            else
            {
                mapped = c;
            }

            if (in_cmd_name && c_idx < COMMAND_MAX_LINE_LENGTH)
            {
                cmd_token[c_idx++] = mapped;
            }

            if ((w_idx + 1U) < sizeof(work_buf))
            {
                work_buf[w_idx++] = mapped;
            }
        }
    }

    cmd_token[c_idx] = '\0';
    work_buf[w_idx]  = '\0';

    /* Execute via cmdline.c table dispatcher */
    status = CmdLineProcess(context, work_buf);

    if (status == CMDLINE_BAD_CMD)
    {
        (void)Command_ReplyError(context, CMD_ERR_E01_UNKNOWN_CMD);
        return (uint8_t)CMD_ERR_E01_UNKNOWN_CMD;
    }

    if (status == CMDLINE_TOO_MANY_ARGS)
    {
        if (!command_is_known_cmd(cmd_token))
        {
            (void)Command_ReplyError(context, CMD_ERR_E01_UNKNOWN_CMD);
            return (uint8_t)CMD_ERR_E01_UNKNOWN_CMD;
        }
        (void)Command_ReplyError(context, CMD_ERR_E02_WRONG_PARAM_CNT);
        return (uint8_t)CMD_ERR_E02_WRONG_PARAM_CNT;
    }

    return status;
}

bool Command_FeedChar(CommandContext *context, char rx_char)
{
    if (context == NULL)
    {
        return false;
    }

    /* Handle LF ('\n') immediately following CR ('\r') in a CR+LF sequence */
    if (rx_char == '\n' && context->last_was_cr)
    {
        context->last_was_cr = false;
        return false;
    }

    /* Trigger line execution on CR ('\r', e.g. Proteus Virtual Terminal or CR+LF)
     * or standalone LF ('\n', Section 4.1). */
    if (rx_char == '\r' || rx_char == '\n')
    {
        context->last_was_cr = (rx_char == '\r');

        /* Account for terminator byte(s) within the 48-byte limit */
        if (context->rx_overflow)
        {
            context->rx_index    = 0U;
            context->rx_overflow = false;
            context->rx_buf[0]   = '\0';
            Command_ClearResponse(context);
            (void)Command_ReplyError(context, CMD_ERR_E06_LINE_TOO_LONG);
            return true;
        }

        context->rx_buf[context->rx_index] = '\0';

        /* Reset collector before executing line */
        context->rx_index    = 0U;
        context->rx_overflow = false;

        (void)Command_ExecuteLine(context, context->rx_buf);
        return true;
    }

    context->last_was_cr = false;

    /* Accumulate character and enforce the 48-byte total line limit
     * (46 payload chars + 2 CR/LF chars = 48 chars max). */
    if (!context->rx_overflow)
    {
        if ((context->rx_index + 2U) <= COMMAND_MAX_LINE_LENGTH)
        {
            context->rx_buf[context->rx_index++] = rx_char;
            context->rx_buf[context->rx_index]   = '\0';
        }
        else
        {
            /* Exceeded 48 characters including terminator -> discard remainder until EOL */
            context->rx_overflow = true;
        }
    }

    return false;
}

