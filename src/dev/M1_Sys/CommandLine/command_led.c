#include "command_led.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../Scheduler/scheduler.h"
#include "../SystemData/system_data.h"
#include "cmdline.h"

/**
 * Parse and validate the blink period string <T> per
 * - T must be an integer in [10 .. 5000] ms.
 * - Returns CMD_ERR_E04_BAD_FORMAT if T contains non-digit characters.
 * - Returns CMD_ERR_E03_OUT_OF_RANGE if T is an integer outside 10..5000.
 */
static Command_ErrorCode_t command_led_parse_period(const char *period_str,
                                                    uint16_t *out_period_ms)
{
    uint32_t acc = 0UL;
    uint8_t  i   = 0U;

    if (period_str == NULL || period_str[0] == '\0' || out_period_ms == NULL)
    {
        return CMD_ERR_E04_BAD_FORMAT;
    }

    /* Check negative integer */
    if (period_str[0] == '-')
    {
        if (period_str[1] == '\0')
        {
            return CMD_ERR_E04_BAD_FORMAT;
        }
        for (i = 1U; period_str[i] != '\0'; i++)
        {
            if (period_str[i] < '0' || period_str[i] > '9')
            {
                return CMD_ERR_E04_BAD_FORMAT;
            }
        }
        return CMD_ERR_E03_OUT_OF_RANGE;
    }

    for (i = 0U; period_str[i] != '\0'; i++)
    {
        if (period_str[i] < '0' || period_str[i] > '9')
        {
            return CMD_ERR_E04_BAD_FORMAT;
        }
        acc = (acc * 10UL) + (uint32_t)(period_str[i] - '0');
        if (acc > 60000UL)
        {
            acc = 60000UL;
        }
    }

    if (acc < (uint32_t)SYSTEM_DATA_BLINK_MIN_MS ||
        acc > (uint32_t)SYSTEM_DATA_BLINK_MAX_MS)
    {
        return CMD_ERR_E03_OUT_OF_RANGE;
    }

    *out_period_ms = (uint16_t)acc;
    return CMD_ERR_NONE;
}

int Cmd_Led(CommandContext *context, int argc, char *argv[])
{
    /* LED requires at least 1 parameter: ON, OFF, or BLINK,<T> */
    if (argc < 2 || Command_IsEmptyArg(argv[1]))
    {
        return Command_ReplyError(context, CMD_ERR_E02_WRONG_PARAM_CNT);
    }

    /* Case 1: LED,ON */
    if (Command_StrEqualsIgnoreCase(argv[1], "ON"))
    {
        if (argc != 2 || Command_HasEmptyArg(argc, argv))
        {
            return Command_ReplyError(context, CMD_ERR_E02_WRONG_PARAM_CNT);
        }

        SCH_TIM_Start(SCH_TIM_LED, 0UL);
        SystemData_SetLedModeOn();
        return Command_ReplyOk(context, "LED,ON");
    }

    /* Case 2: LED,OFF */
    if (Command_StrEqualsIgnoreCase(argv[1], "OFF"))
    {
        if (argc != 2 || Command_HasEmptyArg(argc, argv))
        {
            return Command_ReplyError(context, CMD_ERR_E02_WRONG_PARAM_CNT);
        }

        SCH_TIM_Start(SCH_TIM_LED, 0UL);
        SystemData_SetLedModeOff();
        return Command_ReplyOk(context, "LED,OFF");
    }

    /* Case 3: LED,BLINK,<T> */
    if (Command_StrEqualsIgnoreCase(argv[1], "BLINK"))
    {
        uint16_t            period_ms = 0U;
        Command_ErrorCode_t err;
        char                num_buf[8];
        char                payload[20];
        uint8_t             p_idx = 0U;
        uint8_t             n_idx = 0U;
        const char         *prefix = "LED,BLINK,";

        if (argc != 3 || Command_HasEmptyArg(argc, argv))
        {
            return Command_ReplyError(context, CMD_ERR_E02_WRONG_PARAM_CNT);
        }

        err = command_led_parse_period(argv[2], &period_ms);
        if (err != CMD_ERR_NONE)
        {
            return Command_ReplyError(context, err);
        }

        (void)SystemData_SetLedModeBlink(period_ms);
        SCH_TIM_Start(SCH_TIM_LED, (uint32_t)(period_ms / 2U));

        Command_FormatU16(period_ms, num_buf);
        while (prefix[p_idx] != '\0')
        {
            payload[p_idx] = prefix[p_idx];
            p_idx++;
        }
        while (num_buf[n_idx] != '\0')
        {
            payload[p_idx++] = num_buf[n_idx++];
        }
        payload[p_idx] = '\0';

        return Command_ReplyOk(context, payload);
    }

    /* Unknown LED sub-mode keyword: E02 if wrong param count, E03 if invalid keyword */
    if (argc != 2 || Command_HasEmptyArg(argc, argv))
    {
        return Command_ReplyError(context, CMD_ERR_E02_WRONG_PARAM_CNT);
    }

    return Command_ReplyError(context, CMD_ERR_E03_OUT_OF_RANGE);
}

int Cmd_LedQuery(CommandContext *context, int argc, char *argv[])
{
    SystemData_LedMode mode;

    (void)argv;

    if (argc != 1)
    {
        return Command_ReplyError(context, CMD_ERR_E02_WRONG_PARAM_CNT);
    }

    mode = SystemData_GetLedMode();

    if (mode == SYS_LED_MODE_ON)
    {
        return Command_ReplyOk(context, "LED,ON");
    }

    if (mode == SYS_LED_MODE_BLINK)
    {
        uint16_t    period_ms = SystemData_GetLedBlinkPeriod();
        char        num_buf[8];
        char        payload[20];
        uint8_t     p_idx = 0U;
        uint8_t     n_idx = 0U;
        const char *prefix = "LED,BLINK,";

        Command_FormatU16(period_ms, num_buf);
        while (prefix[p_idx] != '\0')
        {
            payload[p_idx] = prefix[p_idx];
            p_idx++;
        }
        while (num_buf[n_idx] != '\0')
        {
            payload[p_idx++] = num_buf[n_idx++];
        }
        payload[p_idx] = '\0';

        return Command_ReplyOk(context, payload);
    }

    return Command_ReplyOk(context, "LED,OFF");
}

