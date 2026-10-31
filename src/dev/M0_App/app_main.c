#include "app_main.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <stddef.h>

#include "../M1_Sys/Scheduler/scheduler.h"
#include "../M1_Sys/SystemData/system_data.h"
#include "../M2_BSP/TIMER/bsp_timer.h"
#include "../M2_BSP/board.h"

#include "app_clock.h"
#include "app_console.h"
#include "app_lcd.h"
#include "app_led.h"
#include "app_temp.h"

/**
 * RTOS Co-routine task table.
 * Phase offsets (elapsed_ms) are chosen so that I2C tasks (AppClock_Task at
 * even ticks 50k - 10, AppTemp_Task at odd ticks 250m - 125) never execute
 * on the same 1 ms tick, keeping every main-loop pass well under 5 ms .
 */
static const SCH_TaskContext s_app_task_table[] =
{
    /* 1. UART0 Console RX/TX & Command Parser */
    {
        .function   = AppConsole_Task,
        .period_ms  = APP_MAIN_CONSOLE_PERIOD_MS,
        .elapsed_ms = 0U,
        .ready      = TRUE,
        .enabled    = TRUE
    },
    /* 2. LED State & Non-Blocking Blink */
    {
        .function   = AppLed_Task,
        .period_ms  = APP_MAIN_LED_PERIOD_MS,
        .elapsed_ms = 0U,
        .ready      = TRUE,
        .enabled    = TRUE
    },
    /* 3. LCD16x2 Non-Blocking Init & Differential Refresh */
    {
        .function   = AppLcd_Task,
        .period_ms  = APP_MAIN_LCD_PERIOD_MS,
        .elapsed_ms = 0U,
        .ready      = TRUE,
        .enabled    = TRUE
    },
    /* 4. DS3231 RTC Date/Time & Alarm (50 ms step, <= 250 ms poll) */
    {
        .function   = AppClock_Task,
        .period_ms  = APP_MAIN_CLOCK_PERIOD_MS,
        .elapsed_ms = 10U,
        .ready      = TRUE,
        .enabled    = TRUE
    },
    /* 5. DS3231 Internal Temperature Sensor (250 ms step, 1000 ms poll) */
    {
        .function   = AppTemp_Task,
        .period_ms  = APP_MAIN_TEMP_PERIOD_MS,
        .elapsed_ms = 125U,
        .ready      = FALSE,
        .enabled    = TRUE
    }
};

#define APP_MAIN_TASK_COUNT ((uint8_t)(sizeof(s_app_task_table) / sizeof(s_app_task_table[0])))

bool APP_Init(void)
{
    bool ok = true;

    /* Disable interrupts while configuring hardware peripherals and scheduler state */
    cli();

    /* Initialize shared single-context SystemData blackboard */
    SystemData_Init();

    /* Initialize scheduler core and register the Co-routine task table */
    SCH_Initialize();
    if (SCH_InitializeTaskTable(s_app_task_table, APP_MAIN_TASK_COUNT) != SCH_DONE)
    {
        ok = false;
    }

    /* Initialize M0_App peripheral modules */
    if (!AppLed_Init())
    {
        ok = false;
    }

    if (!AppConsole_Init())
    {
        ok = false;
    }

    /* AppClock_Init and AppTemp_Init probe DS3231; if DS3231 is absent at boot,
     * SystemData_HasRtcError() is set so LCD displays "RTC ERROR" (FR-09) while
     * UART and LED commands continue operating normally. */
    (void)AppClock_Init();
    (void)AppTemp_Init();

    if (!AppLcd_Init())
    {
        ok = false;
    }

    /* Start 1 ms Timer0 CTC system tick and enable Cooperative Scheduler */
    if (!BSP_Timer_Init1ms())
    {
        ok = false;
    }

    SCH_StartScheduler();

    /* Enable global interrupts for Timer0 (1 ms tick) and USART0 (RXC / UDRE) */
    sei();

    return ok;
}

void APP_Run(void)
{
    SCH_HandleScheduledTask();
}

void APP_main(void)
{
     if (!APP_Init())
    {
        for (;;)
        {
        }
    }
    while (1)
    {
        APP_Run();
    }
}

