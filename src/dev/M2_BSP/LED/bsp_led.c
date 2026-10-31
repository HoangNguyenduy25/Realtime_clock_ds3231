#include "bsp_led.h"

#include "board.h"
#include "do.h"
#define SRE 7

static do_t s_heartbeat_led =
{
    .port = DO_PORT_B,
    .pin = LED_BLINK_PIN,
    .bStatus = false
};

bool BSP_LED_Init(void)
{
    MCUCR &= (uint8_t)~_BV(SRE);
    s_heartbeat_led.bStatus = false;
    return do_init(&s_heartbeat_led);
}

void BSP_LED_Set(bool on)
{
    if(on)
    {
        do_set(&s_heartbeat_led);
    }
    else 
    {
        do_reset(&s_heartbeat_led);
    }
}

void BSP_LED_Toggle(void)
{
    do_toggle(&s_heartbeat_led);
}