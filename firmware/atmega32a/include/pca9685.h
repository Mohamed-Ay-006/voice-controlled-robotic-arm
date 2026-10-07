#ifndef PCA9685_H
#define PCA9685_H

#include <stdbool.h>
#include <stdint.h>

bool pca9685_init(void);
bool pca9685_set_pulse_us(uint8_t channel, uint16_t pulse_us);
bool pca9685_channel_off(uint8_t channel);
bool pca9685_all_off(void);

#endif

