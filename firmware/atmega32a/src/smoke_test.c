#include "config.h"
#include "pca9685.h"
#include "timebase.h"
#include "twi.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/delay.h>

/*
 * One-shot bench smoke test for ONE unloaded SG90 on PCA9685 CH5.
 * All other servo connectors must be physically unplugged.
 * Sequence: center -> small left move -> small right move -> center -> OFF.
 */
#define TEST_CHANNEL       5U
#define CENTER_PULSE_US 1500U
#define LOW_PULSE_US    1450U
#define HIGH_PULSE_US   1550U

static void delay_ms_bounded(uint16_t milliseconds)
{
    while (milliseconds-- != 0U)
    {
        _delay_ms(1.0);
    }
}

static void shutdown_forever(void)
{
    SERVO_OE_PORT |= (1U << SERVO_OE_PIN);
    (void)pca9685_all_off();
    for (;;)
    {
        /* A power cycle is required to run the one-shot test again. */
    }
}

int main(void)
{
    /* OE HIGH first. External 10k pull-up is still required. */
    SERVO_OE_PORT |= (1U << SERVO_OE_PIN);
    SERVO_OE_DDR |= (1U << SERVO_OE_PIN);

    timebase_init();
    sei();
    twi_init();

    if (!pca9685_init())
    {
        shutdown_forever();
    }

    /* Give the operator time to remove power if wiring is visibly wrong. */
    delay_ms_bounded(1500U);

    if (!pca9685_set_pulse_us(TEST_CHANNEL, CENTER_PULSE_US))
    {
        shutdown_forever();
    }
    SERVO_OE_PORT &= (uint8_t)~(1U << SERVO_OE_PIN);
    delay_ms_bounded(600U);

    if (!pca9685_set_pulse_us(TEST_CHANNEL, LOW_PULSE_US))
    {
        shutdown_forever();
    }
    delay_ms_bounded(500U);

    if (!pca9685_set_pulse_us(TEST_CHANNEL, HIGH_PULSE_US))
    {
        shutdown_forever();
    }
    delay_ms_bounded(500U);

    if (!pca9685_set_pulse_us(TEST_CHANNEL, CENTER_PULSE_US))
    {
        shutdown_forever();
    }
    delay_ms_bounded(500U);

    shutdown_forever();
    return 0;
}

