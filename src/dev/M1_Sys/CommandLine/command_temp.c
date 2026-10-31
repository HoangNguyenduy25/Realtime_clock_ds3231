#include "command_temp.h"

#include <stddef.h>
#include <stdint.h>

#include "../../M0_App/app_temp.h"
#include "cmdline.h"

int Cmd_TempQuery(CommandContext *context, int argc, char *argv[])
{
    DS3231_TempType  temp;
    AppTemp_Status_t status;
    char             val_buf[12];
    char             payload[24];
    uint8_t          p_idx = 0U;
    uint8_t          v_idx = 0U;
    const char      *prefix = "TEMP,";

    (void)argv;

    if (argc != 1)
    {
        return Command_ReplyError(context, CMD_ERR_E02_WRONG_PARAM_CNT);
    }

    status = AppTemp_ReadNow(&temp);
    if (status != APP_TEMP_OK)
    {
        return Command_ReplyError(context, CMD_ERR_E05_RTC_ERROR);
    }

    AppTemp_FormatString(&temp, val_buf, sizeof(val_buf));

    while (prefix[p_idx] != '\0')
    {
        payload[p_idx] = prefix[p_idx];
        p_idx++;
    }
    while (val_buf[v_idx] != '\0' && (p_idx + 3U) < sizeof(payload))
    {
        payload[p_idx++] = val_buf[v_idx++];
    }
    payload[p_idx++] = ',';
    payload[p_idx++] = 'C';
    payload[p_idx]   = '\0';

    return Command_ReplyOk(context, payload);
}

