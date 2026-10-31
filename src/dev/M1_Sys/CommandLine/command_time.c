#include "command_time.h"

#include <stddef.h>
#include <stdint.h>

#include "../../M0_App/app_clock.h"
#include "../SystemData/system_data.h"
#include "cmdline.h"

static Command_ErrorCode_t command_time_map_error(AppClock_Status_t status)
{
    switch (status)
    {
        case APP_CLOCK_ERR_OUT_OF_RANGE:
            return CMD_ERR_E03_OUT_OF_RANGE;
        case APP_CLOCK_ERR_BAD_FORMAT:
            return CMD_ERR_E04_BAD_FORMAT;
        case APP_CLOCK_ERR_RTC_FAIL:
            return CMD_ERR_E05_RTC_ERROR;
        case APP_CLOCK_ERR_INVALID_DATE:
            return CMD_ERR_E07_INVALID_DATE;
        case APP_CLOCK_OK:
        default:
            return CMD_ERR_NONE;
    }
}

static void command_time_append_str(char *dst, uint8_t *idx, uint8_t max_len, const char *src)
{
    uint8_t i = 0U;
    if (dst == NULL || idx == NULL || src == NULL)
    {
        return;
    }
    while (src[i] != '\0' && (*idx + 1U) < max_len)
    {
        dst[*idx] = src[i];
        (*idx)++;
        i++;
    }
    dst[*idx] = '\0';
}

int Cmd_TimeQuery(CommandContext *context, int argc, char *argv[])
{
    uint8_t           hh = 0U, mm = 0U, ss = 0U;
    AppClock_Status_t status;
    char              time_str[10];
    char              payload[20];
    uint8_t           idx = 0U;

    (void)argv;

    if (argc != 1)
    {
        return Command_ReplyError(context, CMD_ERR_E02_WRONG_PARAM_CNT);
    }

    status = AppClock_ReadTime(&hh, &mm, &ss);
    if (status != APP_CLOCK_OK)
    {
        return Command_ReplyError(context, command_time_map_error(status));
    }

    AppClock_FormatTimeStr(hh, mm, ss, time_str);
    command_time_append_str(payload, &idx, sizeof(payload), "TIME,");
    command_time_append_str(payload, &idx, sizeof(payload), time_str);

    return Command_ReplyOk(context, payload);
}

int Cmd_DateQuery(CommandContext *context, int argc, char *argv[])
{
    uint8_t           dd = 1U, mm = 1U, yy = 0U, dow = 1U;
    AppClock_Status_t status;
    char              date_str[12];
    const char       *dow_str;
    char              payload[24];
    uint8_t           idx = 0U;

    (void)argv;

    if (argc != 1)
    {
        return Command_ReplyError(context, CMD_ERR_E02_WRONG_PARAM_CNT);
    }

    status = AppClock_ReadDate(&dd, &mm, &yy, &dow);
    if (status != APP_CLOCK_OK)
    {
        return Command_ReplyError(context, command_time_map_error(status));
    }

    AppClock_FormatDateStr(dd, mm, yy, date_str);
    dow_str = DS3231_DayToString(dow);

    command_time_append_str(payload, &idx, sizeof(payload), "DATE,");
    command_time_append_str(payload, &idx, sizeof(payload), date_str);
    command_time_append_str(payload, &idx, sizeof(payload), ",");
    command_time_append_str(payload, &idx, sizeof(payload), dow_str);

    return Command_ReplyOk(context, payload);
}

int Cmd_DateTimeQuery(CommandContext *context, int argc, char *argv[])
{
    DS3231_DateTimeType dt;
    AppClock_Status_t   status;
    char                date_str[12];
    char                time_str[10];
    const char         *dow_str;
    char                payload[36];
    uint8_t             idx = 0U;

    (void)argv;

    if (argc != 1)
    {
        return Command_ReplyError(context, CMD_ERR_E02_WRONG_PARAM_CNT);
    }

    status = AppClock_ReadDateTime(&dt);
    if (status != APP_CLOCK_OK)
    {
        return Command_ReplyError(context, command_time_map_error(status));
    }

    AppClock_FormatDateStr(dt.date, dt.month, dt.year, date_str);
    AppClock_FormatTimeStr(dt.hours, dt.minutes, dt.seconds, time_str);
    dow_str = DS3231_DayToString(dt.day);

    command_time_append_str(payload, &idx, sizeof(payload), "DT,");
    command_time_append_str(payload, &idx, sizeof(payload), date_str);
    command_time_append_str(payload, &idx, sizeof(payload), ",");
    command_time_append_str(payload, &idx, sizeof(payload), time_str);
    command_time_append_str(payload, &idx, sizeof(payload), ",");
    command_time_append_str(payload, &idx, sizeof(payload), dow_str);

    return Command_ReplyOk(context, payload);
}

int Cmd_TimeSet(CommandContext *context, int argc, char *argv[])
{
    AppClock_Status_t status;
    char              payload[20];
    uint8_t           idx = 0U;

    if (argc != 2 || Command_HasEmptyArg(argc, argv))
    {
        return Command_ReplyError(context, CMD_ERR_E02_WRONG_PARAM_CNT);
    }

    status = AppClock_SetTimeFromStr(argv[1]);
    if (status != APP_CLOCK_OK)
    {
        return Command_ReplyError(context, command_time_map_error(status));
    }

    command_time_append_str(payload, &idx, sizeof(payload), "TIME,");
    command_time_append_str(payload, &idx, sizeof(payload), argv[1]);

    return Command_ReplyOk(context, payload);
}

int Cmd_DateSet(CommandContext *context, int argc, char *argv[])
{
    AppClock_Status_t status;
    const char       *dow_str;
    char              payload[24];
    uint8_t           idx = 0U;

    if (argc != 3 || Command_HasEmptyArg(argc, argv))
    {
        return Command_ReplyError(context, CMD_ERR_E02_WRONG_PARAM_CNT);
    }

    status = AppClock_SetDateFromStr(argv[1], argv[2]);
    if (status != APP_CLOCK_OK)
    {
        return Command_ReplyError(context, command_time_map_error(status));
    }

    dow_str = DS3231_DayToString(DS3231_StringToDay(argv[2]));

    command_time_append_str(payload, &idx, sizeof(payload), "DATE,");
    command_time_append_str(payload, &idx, sizeof(payload), argv[1]);
    command_time_append_str(payload, &idx, sizeof(payload), ",");
    command_time_append_str(payload, &idx, sizeof(payload), dow_str);

    return Command_ReplyOk(context, payload);
}

int Cmd_DateTimeSet(CommandContext *context, int argc, char *argv[])
{
    AppClock_Status_t status;
    const char       *dow_str;
    char              payload[36];
    uint8_t           idx = 0U;

    if (argc != 4 || Command_HasEmptyArg(argc, argv))
    {
        return Command_ReplyError(context, CMD_ERR_E02_WRONG_PARAM_CNT);
    }

    status = AppClock_SetDateTimeFromStr(argv[1], argv[2], argv[3]);
    if (status != APP_CLOCK_OK)
    {
        return Command_ReplyError(context, command_time_map_error(status));
    }

    dow_str = DS3231_DayToString(DS3231_StringToDay(argv[3]));

    command_time_append_str(payload, &idx, sizeof(payload), "DT,");
    command_time_append_str(payload, &idx, sizeof(payload), argv[1]);
    command_time_append_str(payload, &idx, sizeof(payload), ",");
    command_time_append_str(payload, &idx, sizeof(payload), argv[2]);
    command_time_append_str(payload, &idx, sizeof(payload), ",");
    command_time_append_str(payload, &idx, sizeof(payload), dow_str);

    return Command_ReplyOk(context, payload);
}

