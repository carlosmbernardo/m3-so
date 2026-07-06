#include "inode.h"

/* Area de Inodes: tabela estatica com NUM_INODES inodes. */
static inode_t inode_table[NUM_INODES];

/* Bitmap de inodes: 0 = livre, 1 = ocupado. */
static uint8_t inode_bitmap[NUM_INODES];

/* Coloca um inode no estado "vazio" (usado na init e ao alocar/liberar). */
static void inode_reset(uint32_t i)
{
    uint32_t b;
    inode_table[i].size  = 0;
    inode_table[i].type  = INODE_FREE;
    inode_table[i].links = 0;
    for (b = 0; b < DIRECT_BLOCKS; b++)
        inode_table[i].blocks[b] = BLOCK_NONE;
}

/* Inicializa a Area de Inodes: todos livres. */
void inode_init(void)
{
    uint32_t i;
    for (i = 0; i < NUM_INODES; i++) {
        inode_bitmap[i] = 0;
        inode_reset(i);
    }
}

/*
 * Aloca sempre o PRIMEIRO inode livre (menor indice), o que garante a
 * reutilizacao natural de inodes liberados. Retorna o ponteiro para o inode
 * ja limpo, ou 0 se nao houver inode livre.
 */
inode_t *inode_alloc(void)
{
    uint32_t i;
    for (i = 0; i < NUM_INODES; i++) {
        if (inode_bitmap[i] == 0) {
            inode_bitmap[i] = 1;
            inode_reset(i);
            return &inode_table[i];
        }
    }
    return 0;
}

/* Libera um inode, marcando-o como livre no bitmap. */
void inode_free(uint32_t inode)
{
    if (inode >= NUM_INODES)
        return;
    inode_bitmap[inode] = 0;
    inode_reset(inode);
}

/* Retorna o ponteiro para o inode de numero informado. */
inode_t *inode_get(uint32_t inode)
{
    if (inode >= NUM_INODES)
        return 0;
    return &inode_table[inode];
}

/* Retorna o numero do inode a partir do seu ponteiro. */
uint32_t inode_number(inode_t *ip)
{
    return (uint32_t)(ip - inode_table);
}
