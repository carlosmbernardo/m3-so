#ifndef INODE_H
#define INODE_H

#include <stdint.h>
#include "block.h"   /* para DIRECT_BLOCKS usar BLOCK_NONE como sentinela */

/*
 * Gerenciamento de inodes do TreeFS.
 *
 * A "Area de Inodes" e uma tabela estatica em memoria. Cada inode guarda os
 * metadados de um arquivo ou diretorio (tamanho, tipo, referencias e a lista
 * de blocos diretos que armazenam os seus dados).
 */

#define NUM_INODES     64u   /* quantidade total de inodes                 */
#define DIRECT_BLOCKS   8u   /* lista pequena e fixa de blocos diretos     */

/* Tipos de inode (apenas dois, conforme o escopo do trabalho). */
#define INODE_FREE 0u
#define INODE_DIR  1u
#define INODE_FILE 2u

typedef struct {
    uint32_t size;                    /* tamanho do arquivo em bytes        */
    uint32_t type;                    /* INODE_FREE / INODE_DIR / INODE_FILE*/
    uint32_t links;                   /* quantidade de referencias          */
    uint32_t blocks[DIRECT_BLOCKS];   /* lista de blocos diretos            */
} inode_t;

void      inode_init(void);           /* zera tabela e bitmap de inodes     */
inode_t  *inode_alloc(void);          /* reserva 1 inode livre, ou 0        */
void      inode_free(uint32_t inode); /* libera 1 inode                     */
inode_t  *inode_get(uint32_t inode);  /* ponteiro para o inode pelo numero  */
uint32_t  inode_number(inode_t *ip);  /* numero do inode a partir do ptr    */

#endif /* INODE_H */
