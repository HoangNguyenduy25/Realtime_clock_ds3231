#ifndef BSP_LED_H_
#define BSP_LED_H_

#include <stdbool.h>

/* The PB0 heartbeat LED is active high. Initialize it off. */
bool BSP_LED_Init(void);
void BSP_LED_Set(bool on);
void BSP_LED_Toggle(void);

#endif /* BSP_LED_H_ */