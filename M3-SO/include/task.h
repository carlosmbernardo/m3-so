#pragma once
#include <stdint.h>

#define MAX_TASKS 8
#define DEFAULT_STACK_SIZE 2048

typedef struct
{
    uint64_t regs[31];   // x1–x31 (x0 é zero)
    void (*entry)(void);
    int priority;
    uint8_t *stack;

} TCB;

extern TCB tasks[MAX_TASKS];
extern int task_count;

void xTaskCreate(void (*task)(void),
                 uint32_t stack_size,
                 int priority);