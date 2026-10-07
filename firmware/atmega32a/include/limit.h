#ifndef LIMIT_H
#define LIMIT_H

#include <stdbool.h>
#include <stdint.h>

void limit_init(void);
void limit_poll(uint32_t now_ms);
bool limit_active(void);

#endif

