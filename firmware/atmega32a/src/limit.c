#include "limit.h"
#include "config.h"

#include <avr/io.h>

static bool g_raw;
static bool g_stable;
static uint32_t g_changed_at;

void limit_init(void)
{
    LIMIT_DDR &= (uint8_t)~(1U << LIMIT_PIN);
    LIMIT_PORT |= (1U << LIMIT_PIN);
    g_raw = (LIMIT_PIN_REG & (1U << LIMIT_PIN)) == 0U;
    g_stable = g_raw;
    g_changed_at = 0U;
}

void limit_poll(uint32_t now_ms)
{
    bool value = (LIMIT_PIN_REG & (1U << LIMIT_PIN)) == 0U;
    if (value != g_raw)
    {
        g_raw = value;
        g_changed_at = now_ms;
    }

    /* Press inhibits immediately; release must remain stable for 20 ms. */
    if (g_raw)
    {
        g_stable = true;
    }
    else if ((uint32_t)(now_ms - g_changed_at) >= 20UL)
    {
        g_stable = false;
    }
}

bool limit_active(void)
{
    return g_stable;
}
