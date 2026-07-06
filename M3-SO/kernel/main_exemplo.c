/*
 * EXEMPLO de integracao da demonstracao do TreeFS ao kernel.
 *
 * NAO substitua o seu kernel/main.c por este arquivo cegamente.
 * Use-o apenas como referencia. No seu main.c real, faca DUAS coisas:
 *
 *   1) inclua o cabecalho do TreeFS:
 *          #include "treefs.h"
 *
 *   2) apos a inicializacao basica do kernel e ANTES do laco infinito do
 *      scheduler, chame uma unica vez:
 *          treefs_run_demo();
 *
 * O exemplo abaixo mostra a ordem esperada.
 */

#include "treefs.h"

/* Ajuste o nome/assinatura para o ponto de entrada do SEU kernel
 * (ex.: kmain, kernel_main, main...). */
void kmain(void)
{
    /* ... aqui vem a inicializacao ja existente do microkernel:
     *     UART, memoria, tasks, scheduler etc. ... */

    treefs_run_demo();   /* executa os 8 cenarios obrigatorios do TreeFS */

    /* ... aqui o kernel segue para o laco/execucao normal do scheduler ... */
    for (;;) { /* laco infinito do kernel */ }
}
