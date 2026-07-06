#ifndef TREEFS_H
#define TREEFS_H

#include <stdint.h>
#include "inode.h"

/*
 * TreeFS - sistema de arquivos hierarquico baseado em inodes.
 *
 * Arquitetura conceitual:
 *   Aplicacoes -> API de Arquivos -> TreeFS -> Driver de Blocos -> Disco Virtual
 */

#define MAX_NAME 28u   /* tamanho maximo do nome (com terminador incluso) */

/* Entrada de diretorio: nome + numero do inode associado. */
typedef struct {
    char     name[MAX_NAME];
    uint32_t inode;
} dir_entry_t;

/* Superblock: informacoes globais do sistema de arquivos. */
typedef struct {
    uint32_t signature;      /* assinatura do sistema        */
    uint32_t total_blocks;   /* quantidade total de blocos   */
    uint32_t total_inodes;   /* quantidade total de inodes   */
    uint32_t block_size;     /* tamanho de cada bloco        */
} superblock_t;

/* ---- API de arquivos obrigatoria ---- */
int       fs_init(void);
inode_t  *path_lookup(const char *path);
int       mkdir(const char *path);
int       create(const char *path);
int       unlink(const char *path);
int       ls(const char *path);
int       read(int fd, void *buf, uint32_t size);
int       write(int fd, const void *buf, uint32_t size);

/* Demonstracao automatica dos 8 cenarios obrigatorios. */
void      treefs_run_demo(void);

#endif /* TREEFS_H */
