#include "timebase.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>

static volatile uint32_t g_milliseconds;

ISR(TIMER0_COMP_vect)
{
    ++g_milliseconds;
}

void timebase_init(void)
{
    /* 8 MHz / 64 = 125 kHz. OCR0=124 gives an exact 1 ms interrupt. */
    TCCR0 = (1U << WGM01) | (1U << CS01) | (1U << CS00);
    OCR0 = 124U;
    TCNT0 = 0U;
    TIMSK |= (1U << OCIE0);
}

uint32_t timebase_ms(void)
{
    uint32_t value;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        value = g_milliseconds;
    }
    return value;
}

