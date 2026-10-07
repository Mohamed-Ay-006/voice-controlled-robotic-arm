#ifndef UART_H
#define UART_H

#include <stdbool.h>
#include <stdint.h>

void uart_init(void);
bool uart_read(uint8_t *value);
bool uart_rx_error(void);
bool uart_send_line(const char *text);

#endif

