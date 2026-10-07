#ifndef TWI_H
#define TWI_H

#include <stdbool.h>
#include <stdint.h>

void twi_init(void);
bool twi_write_registers(uint8_t address7, uint8_t first_register,
                         const uint8_t *data, uint8_t count);

#endif

