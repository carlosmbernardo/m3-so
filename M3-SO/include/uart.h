#pragma once
#include <stdint.h>

void uart_putc(char c);
void uart_print(const char *s);
void uart_print_uint(uint64_t value);