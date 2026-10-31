#include "bsp_timer.h"

#include <avr/io.h>
#include <avr/interrupt.h>

#include "board.h"
#include "scheduler.h"
/* Timer0 is 8-bit. With the /64 prescaler there are F_CPU / 64000 counts per ms. At 16 MHz that is
 * exactly 250. At 12 MHz it is 187.5, so the integer 187 gives a 1.0027 ms tick, 0.27 % slow. */
#if (F_CPU / 64000UL) == 0UL || (F_CPU / 64000UL) > 256UL
#error Timer0 cannot generate a 1 ms tick with the board clock and /64 prescaler
#endif

bool BSP_Timer_Init1ms(void)
{
    TIMSK0 &= (uint8_t)~_BV(OCIE0A);
    TCCR0A = 0U;
#ifdef TCCR0B
    TCCR0B = 0U;
#endif
    TCNT0 = 0U;
    OCR0A = (uint8_t)((F_CPU / 64000UL) - 1UL);
    TIFR0 = _BV(OCF0A);
#ifdef TCCR0B
    TCCR0A = _BV(WGM01);
    TCCR0B = _BV(CS01) | _BV(CS00);
#else
    TCCR0A = _BV(WGM01) | _BV(CS01) | _BV(CS00);
#endif
    TIMSK0 |= _BV(OCIE0A);
    return true;
}

void BSP_Timer_Deinit(void)
{
    TIMSK0 &= (uint8_t)~_BV(OCIE0A);
    TCCR0A = 0U;
#ifdef TCCR0B
    TCCR0B = 0U;
#endif
}

/* Keep interrupts enabled while updating scheduler counters so Timer2 can
 * sample the software UART without waiting for this lower-rate tick. */
#if defined(TIMER0_COMPA_vect)
ISR(TIMER0_COMPA_vect)
#else
ISR(TIMER0_COMP_vect)
#endif
{
    SCH_RunSystemTickTimer();
}
