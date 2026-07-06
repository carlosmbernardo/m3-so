#include "block.h"

/*
 * Disco virtual: um vetor estatico em memoria com NUM_BLOCKS blocos de
 * BLOCK_SIZE bytes cada. Nenhuma memoria dinamica e utilizada.
 */
static uint8_t disk[NUM_BLOCKS][BLOCK_SIZE];

/* Bitmap de blocos: 0 = livre, 1 = ocupado. */
static uint8_t block_bitmap[NUM_BLOCKS];

/* Limpa o conteudo de um bloco (todos os bytes em zero). */
static void block_clear(uint32_t b)
{
    uint32_t i;
    for (i = 0; i < BLOCK_SIZE; i++)
        disk[b][i] = 0;
}

/* Inicializa o driver: todos os blocos livres e zerados. */
void block_init(void)
{
    uint32_t b;
    for (b = 0; b < NUM_BLOCKS; b++) {
        block_bitmap[b] = 0;
        block_clear(b);
    }
}

/*
 * Aloca sempre o PRIMEIRO bloco livre (menor indice). Isso garante que um
 * bloco liberado seja naturalmente reutilizado em uma alocacao futura.
 * Retorna o numero do bloco ou -1 se nao houver blocos livres.
 */
int block_alloc(void)
{
    uint32_t b;
    for (b = 0; b < NUM_BLOCKS; b++) {
        if (block_bitmap[b] == 0) {
            block_bitmap[b] = 1;
            block_clear(b);
            return (int)b;
        }
    }
    return -1;
}

/* Libera um bloco e limpa o seu conteudo. */
void block_free(uint32_t block)
{
    if (block >= NUM_BLOCKS)
        return;
    block_bitmap[block] = 0;
    block_clear(block);
}

/* Le BLOCK_SIZE bytes do bloco para o buffer do chamador. */
void block_read(uint32_t block, void *buf)
{
    uint8_t *d = (uint8_t *)buf;
    uint32_t i;
    if (block >= NUM_BLOCKS)
        return;
    for (i = 0; i < BLOCK_SIZE; i++)
        d[i] = disk[block][i];
}

/* Escreve BLOCK_SIZE bytes do buffer do chamador no bloco. */
void block_write(uint32_t block, const void *buf)
{
    const uint8_t *s = (const uint8_t *)buf;
    uint32_t i;
    if (block >= NUM_BLOCKS)
        return;
    for (i = 0; i < BLOCK_SIZE; i++)
        disk[block][i] = s[i];
}
