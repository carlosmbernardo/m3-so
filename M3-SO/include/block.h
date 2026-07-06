#ifndef BLOCK_H
#define BLOCK_H

#include <stdint.h>

/*
 * Driver de blocos do TreeFS.
 *
 * O "disco virtual" e um vetor estatico em memoria (nao ha persistencia
 * entre reinicializacoes do QEMU, o que nao e exigido pelo trabalho).
 *
 * Toda a Area de Dados do sistema de arquivos vive dentro deste disco.
 */

#define BLOCK_SIZE   512u   /* tamanho de cada bloco, em bytes            */
#define NUM_BLOCKS   256u   /* quantidade total de blocos do disco virtual*/

/* Valor sentinela para "nenhum bloco". Permite que o bloco 0 seja usavel. */
#define BLOCK_NONE   0xFFFFFFFFu

void block_init(void);                          /* zera disco e bitmap        */
int  block_alloc(void);                         /* reserva 1 bloco livre      */
void block_free(uint32_t block);                /* libera e limpa 1 bloco     */
void block_read(uint32_t block, void *buf);     /* le BLOCK_SIZE bytes        */
void block_write(uint32_t block, const void *buf); /* escreve BLOCK_SIZE bytes */

#endif /* BLOCK_H */
