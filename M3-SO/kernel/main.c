#include "task.h"
#include "scheduler.h"
#include "memory.h"
#include "uart.h"
#include "treefs.h"
extern void uart_print(const char*);
/*   Tasks   */
void task1()
{
    while (1)
    {
        uart_print("Task 1 running\n");
        uart_print("Memory used: ");
        uart_print_uint(memory_used());
        uart_print(" bytes\n");
        uart_print("Memory free: ");
        uart_print_uint(memory_free());
        uart_print(" bytes\n\n");
        yield();
    }
}
void task2()
{
    while (1)
    {
        uart_print("Task 2 running\n");
        uart_print("Memory used: ");
        uart_print_uint(memory_used());
        uart_print(" bytes\n");
        uart_print("Memory free: ");
        uart_print_uint(memory_free());
        uart_print(" bytes\n\n");
        yield();
    }
}
/*   Kernel   */
void kernel_main()
{
    memory_init();   // OBRIGATÓRIO
    uart_print("\n=== Kernel ===\n");

    treefs_run_demo();   // demonstracao do TreeFS (8 cenarios) antes do scheduler

    xTaskCreate(task1, 2048, 1);
    xTaskCreate(task2, 2048, 1);
    scheduler_start();
    while (1);
}
