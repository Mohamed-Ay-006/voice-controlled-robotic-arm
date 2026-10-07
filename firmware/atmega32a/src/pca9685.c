#include "pca9685.h"
#include "config.h"
#include "twi.h"

#include <util/delay.h>

#define REG_MODE1       0x00U
#define REG_MODE2       0x01U
#define REG_LED0_ON_L   0x06U
#define REG_PRESCALE    0xFEU

static bool write_register(uint8_t address, uint8_t value)
{
    return twi_write_registers(PCA9685_ADDRESS, address, &value, 1U);
}

static bool write_channel(uint8_t channel, const uint8_t data[4])
{
    return twi_write_registers(PCA9685_ADDRESS,
                               (uint8_t)(REG_LED0_ON_L + 4U * channel),
                               data, 4U);
}

bool pca9685_channel_off(uint8_t channel)
{
    const uint8_t data[4] = {0U, 0U, 0U, 0x10U};
    if (channel >= 16U)
    {
        return false;
    }
    return write_channel(channel, data);
}

bool pca9685_all_off(void)
{
    uint8_t channel;
    for (channel = 0U; channel < 16U; ++channel)
    {
        if (!pca9685_channel_off(channel))
        {
            return false;
        }
    }
    return true;
}

bool pca9685_init(void)
{
    /* OE must already be HIGH. Sleep before changing PRE_SCALE. */
    if (!write_register(REG_MODE1, 0x10U) ||
        !write_register(REG_MODE2, 0x04U) ||
        !write_register(REG_PRESCALE, PCA9685_PRESCALE))
    {
        return false;
    }

    /* Wake with auto-increment, wait for oscillator, then issue RESTART. */
    if (!write_register(REG_MODE1, 0x20U))
    {
        return false;
    }
    _delay_us(500.0);
    if (!write_register(REG_MODE1, 0xA0U))
    {
        return false;
    }
    return pca9685_all_off();
}

bool pca9685_set_pulse_us(uint8_t channel, uint16_t pulse_us)
{
    uint16_t ticks;
    uint8_t data[4];

    if ((channel >= 16U) || (pulse_us < 400U) || (pulse_us > 2600U))
    {
        return false;
    }

    /* Tick = (PRESCALE+1)/25 us for the nominal 25 MHz oscillator. */
    ticks = (uint16_t)(((uint32_t)pulse_us * 25UL +
                        ((PCA9685_PRESCALE + 1U) / 2U)) /
                       (PCA9685_PRESCALE + 1U));
    if (ticks > 4095U)
    {
        ticks = 4095U;
    }

    data[0] = 0U;
    data[1] = 0U;
    data[2] = (uint8_t)ticks;
    data[3] = (uint8_t)((ticks >> 8U) & 0x0FU);
    return write_channel(channel, data);
}

