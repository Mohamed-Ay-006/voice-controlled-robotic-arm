#include "uart.h"
#include "config.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <string.h>
#include <util/atomic.h>

#define RX_CAPACITY 64U
#define TX_CAPACITY 128U

static volatile uint8_t g_rx[RX_CAPACITY];
static volatile uint8_t g_rx_head;
static volatile uint8_t g_rx_tail;
static volatile uint8_t g_tx[TX_CAPACITY];
static volatile uint8_t g_tx_head;
static volatile uint8_t g_tx_tail;
static volatile uint8_t g_rx_fault;

ISR(USART_RXC_vect)
{
    uint8_t status = UCSRA;
    uint8_t value = UDR;
    uint8_t next = (uint8_t)((g_rx_head + 1U) & (RX_CAPACITY - 1U));

    if ((status & ((1U << FE) | (1U << DOR) | (1U << PE))) != 0U)
    {
        g_rx_fault = 1U;
        return;
    }
    if (next == g_rx_tail)
    {
        g_rx_fault = 1U;
        return;
    }
    g_rx[g_rx_head] = value;
    g_rx_head = next;
}

ISR(USART_UDRE_vect)
{
    if (g_tx_tail == g_tx_head)
    {
        UCSRB &= (uint8_t)~(1U << UDRIE);
        return;
    }
    UDR = g_tx[g_tx_tail];
    g_tx_tail = (uint8_t)((g_tx_tail + 1U) & (TX_CAPACITY - 1U));
}

void uart_init(void)
{
    /* UBRR=51, actual rate 9615.38 baud, error +0.16%. */
    const uint16_t ubrr = (uint16_t)((F_CPU + (8UL * UART_BAUD)) /
                                     (16UL * UART_BAUD) - 1UL);
    UCSRA = 0U;
    UBRRH = (uint8_t)(ubrr >> 8U);
    UBRRL = (uint8_t)ubrr;
    UCSRC = (1U << URSEL) | (1U << UCSZ1) | (1U << UCSZ0);
    UCSRB = (1U << RXEN) | (1U << TXEN) | (1U << RXCIE);
}

bool uart_read(uint8_t *value)
{
    if ((value == 0) || (g_rx_tail == g_rx_head))
    {
        return false;
    }
    *value = g_rx[g_rx_tail];
    g_rx_tail = (uint8_t)((g_rx_tail + 1U) & (RX_CAPACITY - 1U));
    return true;
}

bool uart_rx_error(void)
{
    uint8_t fault;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        fault = g_rx_fault;
        g_rx_fault = 0U;
        if (fault != 0U)
        {
            g_rx_tail = g_rx_head;
        }
    }
    return fault != 0U;
}

bool uart_send_line(const char *text)
{
    uint8_t head;
    uint8_t free_bytes;
    size_t length;
    size_t index;

    if (text == 0)
    {
        return false;
    }
    length = strlen(text);
    if (length > (TX_CAPACITY - 2U))
    {
        return false;
    }

    head = g_tx_head;
    free_bytes = (uint8_t)((g_tx_tail - head - 1U) & (TX_CAPACITY - 1U));
    if (free_bytes < (uint8_t)(length + 1U))
    {
        return false;
    }

    for (index = 0U; index < length; ++index)
    {
        g_tx[head] = (uint8_t)text[index];
        head = (uint8_t)((head + 1U) & (TX_CAPACITY - 1U));
    }
    g_tx[head] = (uint8_t)'\n';
    head = (uint8_t)((head + 1U) & (TX_CAPACITY - 1U));

    ATOMIC_BLOCK(ATOMIC_RESTORESTATE)
    {
        g_tx_head = head;
        UCSRB |= (1U << UDRIE);
    }
    return true;
}

