#include "twi.h"
#include "config.h"
#include "timebase.h"

#include <avr/io.h>

#define TWI_TIMEOUT_MS 5UL

static bool wait_for_interrupt(void)
{
    uint32_t started = timebase_ms();
    while ((TWCR & (1U << TWINT)) == 0U)
    {
        if ((uint32_t)(timebase_ms() - started) >= TWI_TIMEOUT_MS)
        {
            TWCR = 0U;
            TWCR = (1U << TWEN);
            return false;
        }
    }
    return true;
}

static uint8_t status_code(void)
{
    return (uint8_t)(TWSR & 0xF8U);
}

static bool start_condition(void)
{
    uint8_t status;
    TWCR = (1U << TWINT) | (1U << TWSTA) | (1U << TWEN);
    if (!wait_for_interrupt())
    {
        return false;
    }
    status = status_code();
    return (status == 0x08U) || (status == 0x10U);
}

static bool write_byte(uint8_t value, uint8_t expected_status)
{
    TWDR = value;
    TWCR = (1U << TWINT) | (1U << TWEN);
    return wait_for_interrupt() && (status_code() == expected_status);
}

static bool stop_condition(void)
{
    uint32_t started = timebase_ms();
    TWCR = (1U << TWINT) | (1U << TWEN) | (1U << TWSTO);
    while ((TWCR & (1U << TWSTO)) != 0U)
    {
        if ((uint32_t)(timebase_ms() - started) >= TWI_TIMEOUT_MS)
        {
            TWCR = 0U;
            TWCR = (1U << TWEN);
            return false;
        }
    }
    return true;
}

void twi_init(void)
{
    /* SCL = F_CPU / (16 + 2*TWBR*prescaler). At 8 MHz/100 kHz, TWBR=32. */
    TWSR = 0U;
    TWBR = (uint8_t)((F_CPU / TWI_HZ - 16UL) / 2UL);
    TWCR = (1U << TWEN);
}

bool twi_write_registers(uint8_t address7, uint8_t first_register,
                         const uint8_t *data, uint8_t count)
{
    bool ok = true;
    uint8_t index;

    if ((address7 > 0x7FU) || (data == 0) || (count == 0U))
    {
        return false;
    }

    ok = start_condition();
    if (ok)
    {
        ok = write_byte((uint8_t)(address7 << 1U), 0x18U);
    }
    if (ok)
    {
        ok = write_byte(first_register, 0x28U);
    }
    for (index = 0U; ok && (index < count); ++index)
    {
        ok = write_byte(data[index], 0x28U);
    }
    if (!stop_condition())
    {
        ok = false;
    }
    return ok;
}

