#ifndef APP_MAIN_H_
#define APP_MAIN_H_

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Cooperative Scheduler Co-routine task periods (in milliseconds):
 * - Console (UART0 RX/TX): 1 ms  (fast response <= 100 ms)
 * - LED (non-blocking blink via SCH_TIM_LED): 1 ms (10..5000 ms period support)
 * - LCD (HD44780): 2 ms (differential update)
 * - Clock (DS3231 RTC): 50 ms (polls DS3231 every 250 ms)
 * - Temp (DS3231 temperature sensor): 250 ms (polls sensor every 1000 ms)
 */
#define APP_MAIN_CONSOLE_PERIOD_MS  1U
#define APP_MAIN_LED_PERIOD_MS      1U
#define APP_MAIN_LCD_PERIOD_MS      2U
#define APP_MAIN_CLOCK_PERIOD_MS    50U
#define APP_MAIN_TEMP_PERIOD_MS     250U

/**
 * Initialize SystemData, Cooperative Scheduler, 1 ms Timer0 tick, and all M0_App
 * peripheral modules (LED, Console/UART0, Clock/DS3231, Temp, LCD16x2).
 *
 * @return true if all core subsystems and the scheduler initialized successfully.
 */
bool APP_Init(void);

/**
 * Execute one cooperative scheduler dispatch pass in main-loop context.
 */
void APP_Run(void);

/**
 * Main application entry point called by main() in src/main.c.
 * Initializes the system, enables global interrupts, and runs the non-blocking
 * cooperative scheduler loop indefinitely.
 */
void APP_main(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_MAIN_H_ */

