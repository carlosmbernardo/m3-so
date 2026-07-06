/* Harness SOMENTE para verificacao no host (nao faz parte do kernel).
 * Executa exatamente a mesma logica do TreeFS que roda no RISC-V/QEMU;
 * a unica diferenca e que tfs_putc() escreve em stdout (HOST_TEST). */
#include "treefs.h"

int main(void)
{
    treefs_run_demo();
    return 0;
}
