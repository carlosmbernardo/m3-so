#include <stdint.h>

#define UART0 0x10000000L

extern void uart_putc(char);

/* Imprime número decimal */
void uart_print_uint(uint64_t value)
{
    char buf[32];
    int i = 0;

    if (value == 0)
    {
        uart_putc('0');
        return;
    }

    while (value > 0)
    {
        buf[i++] = '0' + (value % 10);
        value /= 10;
    }

    while (i--)
        uart_putc(buf[i]);
}


void uart_putc(char c)
{
    *(volatile uint8_t*)UART0 = c;
}

void uart_print(const char *s)
{
    while (*s)
        uart_putc(*s++);
}