#include "task.h"
#include "memory.h"
#include <stdint.h>

TCB tasks[MAX_TASKS];
int task_count = 0;

void xTaskCreate(void (*task)(void),
                 uint32_t stack_size,
                 int priority)
{
    if (task_count >= MAX_TASKS)
        return;

    if (stack_size == 0)
        stack_size = DEFAULT_STACK_SIZE;

    TCB *t = &tasks[task_count];

    t->stack = (uint8_t*)kmalloc(stack_size);

    uint64_t *sp = (uint64_t*)(t->stack + stack_size);

    /* Configurar contexto inicial */

    t->regs[0] = (uint64_t)task;   // ra
    t->regs[1] = (uint64_t)sp;     // sp

    t->entry = task;
    t->priority = priority;

    task_count++;
}