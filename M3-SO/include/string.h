#pragma once
#include <stdint.h>

void *memcpy(void *dest, const void *src, uint64_t n);
void *memset(void *s, int c, uint64_t n);

int strcmp(const char *a, const char *b);
char *strcpy(char *dest, const char *src);
uint64_t strlen(const char *s);