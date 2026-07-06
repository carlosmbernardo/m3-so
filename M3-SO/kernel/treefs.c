#include "treefs.h"
#include "inode.h"
#include "block.h"

/* ======================================================================
 * Constantes internas
 * ====================================================================== */

#define TREEFS_MAGIC 0x54524546u   /* "TREF" - assinatura do superblock */
#define MAX_PATH     128u          /* tamanho maximo de um caminho      */
#define MAX_FDS      16            /* tabela minima de descritores      */

/* Quantidade de entradas que cabem em um unico bloco de diretorio. */
#define ENTRIES_PER_DIR (BLOCK_SIZE / sizeof(dir_entry_t))

/* ======================================================================
 * Estado global do TreeFS
 * ====================================================================== */

static superblock_t sb;             /* superblock                        */
static int fd_table[MAX_FDS];       /* fd -> numero do inode, ou -1 livre */
#define ROOT_INO 0u                 /* inode raiz e sempre o de numero 0 */

/* ======================================================================
 * Saida pela UART (ou stdout no teste de host)
 * ====================================================================== */

#ifdef HOST_TEST
#include <stdio.h>
static void tfs_putc(char c) { putchar(c); }
#else
/* QEMU 'virt' expoe uma UART NS16550 em 0x10000000. */
#define UART0_BASE 0x10000000UL
static void tfs_putc(char c)
{
    volatile unsigned char *thr = (volatile unsigned char *)(UART0_BASE + 0);
    volatile unsigned char *lsr = (volatile unsigned char *)(UART0_BASE + 5);
    while ((*lsr & 0x20u) == 0) { /* aguarda o transmissor ficar livre */ }
    *thr = (unsigned char)c;
}
#endif

static void tfs_puts(const char *s)
{
    while (*s)
        tfs_putc(*s++);
}

/* Imprime um inteiro sem sinal em decimal. */
static void tfs_putu(uint32_t v)
{
    char buf[12];
    int n = 0;
    if (v == 0) { tfs_putc('0'); return; }
    while (v) { buf[n++] = (char)('0' + (v % 10)); v /= 10; }
    while (n--) tfs_putc(buf[n]);
}

/* ======================================================================
 * Pequenos helpers de string (sem libc, freestanding)
 * ====================================================================== */

static uint32_t tfs_strlen(const char *s)
{
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}

static int tfs_streq(const char *a, const char *b)
{
    uint32_t i = 0;
    while (a[i] && b[i]) {
        if (a[i] != b[i]) return 0;
        i++;
    }
    return a[i] == b[i];
}

static void tfs_strcpy(char *d, const char *s)
{
    uint32_t i = 0;
    while (s[i]) { d[i] = s[i]; i++; }
    d[i] = 0;
}

/* ======================================================================
 * Operacoes sobre entradas de diretorio
 *
 * Cada diretorio usa exatamente um bloco de dados (blocks[0]) que armazena
 * um vetor de dir_entry_t. Uma entrada esta livre quando name[0] == 0.
 * ====================================================================== */

/* Procura 'name' no diretorio. Retorna o indice da entrada ou -1. */
static int dir_find(inode_t *dir, const char *name, uint32_t *ino_out)
{
    dir_entry_t entries[ENTRIES_PER_DIR];
    uint32_t i;
    block_read(dir->blocks[0], entries);
    for (i = 0; i < ENTRIES_PER_DIR; i++) {
        if (entries[i].name[0] != 0 && tfs_streq(entries[i].name, name)) {
            if (ino_out) *ino_out = entries[i].inode;
            return (int)i;
        }
    }
    return -1;
}

/* Adiciona uma entrada (name -> ino) no diretorio. Retorna 0 ou -1 (cheio). */
static int dir_add(inode_t *dir, const char *name, uint32_t ino)
{
    dir_entry_t entries[ENTRIES_PER_DIR];
    uint32_t i;
    block_read(dir->blocks[0], entries);
    for (i = 0; i < ENTRIES_PER_DIR; i++) {
        if (entries[i].name[0] == 0) {
            tfs_strcpy(entries[i].name, name);
            entries[i].inode = ino;
            block_write(dir->blocks[0], entries);
            return 0;
        }
    }
    return -1;
}

/*
 * Remove a entrada 'name' do diretorio, substituindo-a pela ultima entrada
 * valida (compactacao simples). Retorna 0 ou -1 se nao encontrada.
 */
static int dir_remove(inode_t *dir, const char *name)
{
    dir_entry_t entries[ENTRIES_PER_DIR];
    uint32_t i;
    int idx = -1, last = -1;
    block_read(dir->blocks[0], entries);
    for (i = 0; i < ENTRIES_PER_DIR; i++) {
        if (entries[i].name[0] != 0) {
            last = (int)i;
            if (tfs_streq(entries[i].name, name))
                idx = (int)i;
        }
    }
    if (idx < 0)
        return -1;
    if (idx != last) {
        tfs_strcpy(entries[idx].name, entries[last].name);
        entries[idx].inode = entries[last].inode;
    }
    entries[last].name[0] = 0;
    entries[last].inode = 0;
    block_write(dir->blocks[0], entries);
    return 0;
}

/* ======================================================================
 * Resolucao de caminhos
 * ====================================================================== */

/*
 * path_lookup: percorre um caminho ABSOLUTO componente a componente a partir
 * da raiz e retorna o inode final, ou 0 (NULL) se o caminho nao existir.
 * Nao usa strtok: o parser percorre a string caractere a caractere.
 */
inode_t *path_lookup(const char *path)
{
    inode_t *cur;
    uint32_t i;
    char comp[MAX_NAME];

    if (path == 0 || path[0] != '/')   /* somente caminhos absolutos */
        return 0;

    cur = inode_get(ROOT_INO);         /* comeca na raiz */
    if (!cur)
        return 0;

    i = 1;                             /* pula a '/' inicial */
    while (path[i] != 0) {
        uint32_t n = 0;
        uint32_t ino;

        /* extrai o proximo componente ate '/' ou fim da string */
        while (path[i] != 0 && path[i] != '/') {
            if (n < MAX_NAME - 1)
                comp[n++] = path[i];
            i++;
        }
        comp[n] = 0;
        if (path[i] == '/')
            i++;
        if (n == 0)                    /* ignora '//' ou '/' final */
            continue;

        if (cur->type != INODE_DIR)    /* intermediario deve ser diretorio */
            return 0;
        if (dir_find(cur, comp, &ino) < 0)
            return 0;                  /* componente nao encontrado */
        cur = inode_get(ino);
        if (!cur)
            return 0;
    }
    return cur;
}

/*
 * resolve_parent: dado um caminho, encontra o inode do diretorio PAI e copia
 * o nome do ultimo componente em 'name_out'. Usado por mkdir/create/unlink.
 * Retorna o inode do pai ou 0 em erro.
 */
static inode_t *resolve_parent(const char *path, char *name_out)
{
    int len, last_slash, i, nlen;
    char parent[MAX_PATH];

    if (path == 0 || path[0] != '/')
        return 0;

    len = (int)tfs_strlen(path);

    /* localiza a ultima '/' */
    last_slash = -1;
    for (i = 0; i < len; i++)
        if (path[i] == '/')
            last_slash = i;
    if (last_slash < 0)
        return 0;

    /* nome = tudo depois da ultima '/' */
    nlen = len - (last_slash + 1);
    if (nlen <= 0 || nlen >= (int)MAX_NAME)
        return 0;
    for (i = 0; i < nlen; i++)
        name_out[i] = path[last_slash + 1 + i];
    name_out[nlen] = 0;

    /* caminho do pai */
    if (last_slash == 0) {             /* pai e a raiz, ex.: "/home" */
        parent[0] = '/';
        parent[1] = 0;
    } else {
        for (i = 0; i < last_slash; i++)
            parent[i] = path[i];
        parent[last_slash] = 0;
    }
    return path_lookup(parent);
}

/* ======================================================================
 * Inicializacao do sistema de arquivos
 * ====================================================================== */

int fs_init(void)
{
    inode_t *root;
    int rblk, i;

    block_init();                      /* driver de blocos */
    inode_init();                      /* limpa bitmap e area de inodes */
    for (i = 0; i < MAX_FDS; i++)      /* tabela de descritores */
        fd_table[i] = -1;

    /* superblock */
    sb.signature    = TREEFS_MAGIC;
    sb.total_blocks = NUM_BLOCKS;
    sb.total_inodes = NUM_INODES;
    sb.block_size   = BLOCK_SIZE;

    /* inode raiz (sera o inode 0) */
    root = inode_alloc();
    if (!root)
        return -1;
    rblk = block_alloc();              /* bloco de entradas da raiz */
    if (rblk < 0)
        return -2;
    root->type     = INODE_DIR;
    root->size     = 0;
    root->links    = 1;
    root->blocks[0] = (uint32_t)rblk;

    /* estrutura inicial obrigatoria: /home, /tmp, /bin */
    if (mkdir("/home") != 0) return -3;
    if (mkdir("/tmp")  != 0) return -4;
    if (mkdir("/bin")  != 0) return -5;

    return 0;
}

/* ======================================================================
 * Criacao de diretorios
 * ====================================================================== */

int mkdir(const char *path)
{
    char name[MAX_NAME];
    inode_t *parent, *ip;
    uint32_t dummy;
    int blk;

    parent = resolve_parent(path, name);
    if (!parent)                        return -1;
    if (parent->type != INODE_DIR)      return -2;
    if (dir_find(parent, name, &dummy) >= 0) return -3; /* duplicidade */

    ip = inode_alloc();
    if (!ip)                            return -4;
    blk = block_alloc();
    if (blk < 0) { inode_free(inode_number(ip)); return -5; }

    ip->type     = INODE_DIR;
    ip->size     = 0;
    ip->links    = 1;
    ip->blocks[0] = (uint32_t)blk;      /* bloco de entradas (ja zerado) */

    if (dir_add(parent, name, inode_number(ip)) != 0) {
        block_free((uint32_t)blk);
        inode_free(inode_number(ip));
        return -6;
    }
    return 0;
}

/* ======================================================================
 * Criacao de arquivos e descritor de arquivo
 * ====================================================================== */

int create(const char *path)
{
    char name[MAX_NAME];
    inode_t *parent, *ip;
    uint32_t dummy;
    int fd;

    parent = resolve_parent(path, name);
    if (!parent)                        return -1;
    if (parent->type != INODE_DIR)      return -2;
    if (dir_find(parent, name, &dummy) >= 0) return -3; /* duplicidade */

    ip = inode_alloc();
    if (!ip)                            return -4;
    ip->type  = INODE_FILE;
    ip->size  = 0;
    ip->links = 1;
    /* blocks[] ja vem como BLOCK_NONE do inode_alloc (lista vazia) */

    if (dir_add(parent, name, inode_number(ip)) != 0) {
        inode_free(inode_number(ip));
        return -5;
    }

    /* reserva uma posicao livre na tabela de descritores */
    for (fd = 0; fd < MAX_FDS; fd++) {
        if (fd_table[fd] < 0) {
            fd_table[fd] = (int)inode_number(ip);
            return fd;
        }
    }
    return -6;                          /* sem descritor livre */
}

/* ======================================================================
 * Escrita de arquivos
 * ====================================================================== */

int write(int fd, const void *buf, uint32_t size)
{
    inode_t *ip;
    const uint8_t *src = (const uint8_t *)buf;
    uint32_t maxbytes, nblocks, written, i, k;

    if (fd < 0 || fd >= MAX_FDS || fd_table[fd] < 0)
        return -1;
    ip = inode_get((uint32_t)fd_table[fd]);
    if (!ip || ip->type != INODE_FILE)
        return -2;

    maxbytes = DIRECT_BLOCKS * BLOCK_SIZE;
    if (size > maxbytes)                /* nao cabe nos blocos diretos */
        return -3;

    /* substitui todo o conteudo: libera blocos antigos */
    for (i = 0; i < DIRECT_BLOCKS; i++) {
        if (ip->blocks[i] != BLOCK_NONE) {
            block_free(ip->blocks[i]);
            ip->blocks[i] = BLOCK_NONE;
        }
    }

    nblocks = (size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    written = 0;
    for (i = 0; i < nblocks; i++) {
        uint8_t tmp[BLOCK_SIZE];
        uint32_t chunk;
        int blk = block_alloc();
        if (blk < 0) {                  /* rollback em caso de falta */
            uint32_t j;
            for (j = 0; j < i; j++) {
                block_free(ip->blocks[j]);
                ip->blocks[j] = BLOCK_NONE;
            }
            ip->size = 0;
            return -4;
        }
        ip->blocks[i] = (uint32_t)blk;

        for (k = 0; k < BLOCK_SIZE; k++) /* limpa o buffer temporario */
            tmp[k] = 0;
        chunk = size - written;
        if (chunk > BLOCK_SIZE)
            chunk = BLOCK_SIZE;
        for (k = 0; k < chunk; k++)      /* copia so a parte necessaria */
            tmp[k] = src[written + k];
        block_write((uint32_t)blk, tmp); /* escreve o bloco completo */
        written += chunk;
    }

    ip->size = size;
    return (int)size;
}

/* ======================================================================
 * Leitura de arquivos
 * ====================================================================== */

int read(int fd, void *buf, uint32_t size)
{
    inode_t *ip;
    uint8_t *dst = (uint8_t *)buf;
    uint32_t toread, done, i, k;

    if (fd < 0 || fd >= MAX_FDS || fd_table[fd] < 0)
        return -1;
    ip = inode_get((uint32_t)fd_table[fd]);
    if (!ip || ip->type != INODE_FILE)
        return -2;

    toread = size;
    if (toread > ip->size)             /* nunca le alem do tamanho real */
        toread = ip->size;

    done = 0;
    for (i = 0; i < DIRECT_BLOCKS && done < toread; i++) {
        uint8_t tmp[BLOCK_SIZE];
        uint32_t chunk;
        if (ip->blocks[i] == BLOCK_NONE)
            break;
        block_read(ip->blocks[i], tmp);
        chunk = toread - done;
        if (chunk > BLOCK_SIZE)
            chunk = BLOCK_SIZE;
        for (k = 0; k < chunk; k++)
            dst[done + k] = tmp[k];
        done += chunk;
    }
    return (int)done;
}

/* ======================================================================
 * Remocao de arquivos
 * ====================================================================== */

int unlink(const char *path)
{
    char name[MAX_NAME];
    inode_t *parent, *ip;
    uint32_t ino, i;
    int fd;

    parent = resolve_parent(path, name);
    if (!parent)                   return -1;
    if (parent->type != INODE_DIR) return -2;
    if (dir_find(parent, name, &ino) < 0) return -3;

    ip = inode_get(ino);
    if (!ip)                       return -4;
    if (ip->type != INODE_FILE)    return -5;  /* unlink so remove arquivos */

    /* libera todos os blocos do arquivo */
    for (i = 0; i < DIRECT_BLOCKS; i++) {
        if (ip->blocks[i] != BLOCK_NONE) {
            block_free(ip->blocks[i]);
            ip->blocks[i] = BLOCK_NONE;
        }
    }
    /* invalida descritores que apontam para este inode */
    for (fd = 0; fd < MAX_FDS; fd++)
        if (fd_table[fd] == (int)ino)
            fd_table[fd] = -1;

    inode_free(ino);               /* libera o inode  */
    dir_remove(parent, name);      /* remove a entrada do diretorio pai */
    return 0;
}

/* ======================================================================
 * Listagem de diretorios
 * ====================================================================== */

int ls(const char *path)
{
    inode_t *ip;
    dir_entry_t entries[ENTRIES_PER_DIR];
    uint32_t i;

    ip = path_lookup(path);
    if (!ip)                    return -1;
    if (ip->type != INODE_DIR)  return -2;

    block_read(ip->blocks[0], entries);
    for (i = 0; i < ENTRIES_PER_DIR; i++) {
        if (entries[i].name[0] != 0) {
            tfs_puts(entries[i].name);
            tfs_putc('\n');
        }
    }
    return 0;
}

/* ======================================================================
 * Demonstracao automatica dos 8 cenarios obrigatorios
 * ====================================================================== */

static void banner(const char *s)
{
    tfs_puts("\n");
    tfs_puts(s);
    tfs_puts("\n");
}

void treefs_run_demo(void)
{
    inode_t *old_ip, *new_ip;
    uint32_t old_block, new_block;
    int r, fd, w, rd, fd2, w2;
    int c1, c2, c3, c4, c5, c6, c7;
    int reuse_inode, reuse_block;
    char buffer[64];
    int i;

    tfs_puts("\n========== DEMONSTRACAO TreeFS ==========\n");

    /* ---- CENARIO 1: listagem da raiz ---- */
    banner("[CENARIO 1] fs_init(); ls(\"/\")");
    fs_init();
    ls("/");
    c1 = (path_lookup("/home") && path_lookup("/tmp") && path_lookup("/bin")) ? 1 : 0;
    tfs_puts("Resultado: "); tfs_puts(c1 ? "PASS" : "FAIL"); tfs_puts("\n");

    /* ---- CENARIO 2: criacao de diretorio ---- */
    banner("[CENARIO 2] mkdir(\"/home/aluno\")");
    r = mkdir("/home/aluno");
    c2 = (r == 0 && path_lookup("/home/aluno") != 0) ? 1 : 0;
    tfs_puts("retorno mkdir = "); tfs_putu((uint32_t)r); tfs_putc('\n');
    tfs_puts("Resultado: "); tfs_puts(c2 ? "PASS" : "FAIL"); tfs_puts("\n");

    /* ---- CENARIO 3: criacao de arquivo ---- */
    banner("[CENARIO 3] create(\"/home/aluno/notas.txt\")");
    fd = create("/home/aluno/notas.txt");
    c3 = (fd >= 0 && path_lookup("/home/aluno/notas.txt") != 0) ? 1 : 0;
    tfs_puts("fd = "); tfs_putu((uint32_t)fd); tfs_putc('\n');
    tfs_puts("Resultado: "); tfs_puts(c3 ? "PASS" : "FAIL"); tfs_puts("\n");

    /* ---- CENARIO 4: escrita ---- */
    banner("[CENARIO 4] write(fd, \"Sistemas Operacionais\", 22)");
    w = write(fd, "Sistemas Operacionais", 22);
    c4 = (w == 22) ? 1 : 0;
    tfs_puts("bytes escritos = "); tfs_putu((uint32_t)w); tfs_putc('\n');
    tfs_puts("Resultado: "); tfs_puts(c4 ? "PASS" : "FAIL"); tfs_puts("\n");

    /* ---- CENARIO 5: leitura ---- */
    banner("[CENARIO 5] read(fd, buffer, 22)");
    for (i = 0; i < 64; i++) buffer[i] = 0;
    rd = read(fd, buffer, 22);
    tfs_puts("bytes lidos = "); tfs_putu((uint32_t)rd); tfs_putc('\n');
    tfs_puts("conteudo = \""); tfs_puts(buffer); tfs_puts("\"\n");
    c5 = (rd == 22 && tfs_streq(buffer, "Sistemas Operacionais")) ? 1 : 0;
    tfs_puts("Resultado: "); tfs_puts(c5 ? "PASS" : "FAIL"); tfs_puts("\n");

    /* guarda inode e primeiro bloco ANTES de remover (para o cenario 8) */
    old_ip = path_lookup("/home/aluno/notas.txt");
    old_block = old_ip ? old_ip->blocks[0] : BLOCK_NONE;

    /* ---- CENARIO 6: remocao ---- */
    banner("[CENARIO 6] unlink(\"/home/aluno/notas.txt\")");
    r = unlink("/home/aluno/notas.txt");
    c6 = (r == 0 && path_lookup("/home/aluno/notas.txt") == 0) ? 1 : 0;
    tfs_puts("retorno unlink = "); tfs_putu((uint32_t)r); tfs_putc('\n');
    tfs_puts("path_lookup apos unlink = ");
    tfs_puts(path_lookup("/home/aluno/notas.txt") == 0 ? "NULL" : "NAO-NULL");
    tfs_putc('\n');
    tfs_puts("Resultado: "); tfs_puts(c6 ? "PASS" : "FAIL"); tfs_puts("\n");

    /* ---- CENARIO 7: navegacao hierarquica ---- */
    banner("[CENARIO 7] ls(\"/home\") e ls(\"/home/aluno\")");
    tfs_puts("ls(\"/home\"):\n");
    ls("/home");
    tfs_puts("ls(\"/home/aluno\"):\n");
    ls("/home/aluno");
    c7 = (path_lookup("/home/aluno") != 0) ? 1 : 0;
    tfs_puts("Resultado: "); tfs_puts(c7 ? "PASS" : "FAIL"); tfs_puts("\n");

    /* ---- CENARIO 8: reutilizacao de inode e bloco ---- */
    banner("[CENARIO 8] Reutilizacao de inode e bloco");
    fd2 = create("/home/aluno/reuso.txt");
    new_ip = path_lookup("/home/aluno/reuso.txt");
    w2 = write(fd2, "Reutilizacao TreeFS", 20);
    new_block = new_ip ? new_ip->blocks[0] : BLOCK_NONE;
    (void)w2;

    reuse_inode = (new_ip == old_ip) ? 1 : 0;
    reuse_block = (new_block == old_block && old_block != BLOCK_NONE) ? 1 : 0;

    tfs_puts("inode liberado (slot) = "); tfs_putu(inode_number(old_ip)); tfs_putc('\n');
    tfs_puts("inode reusado  (slot) = "); tfs_putu(inode_number(new_ip)); tfs_putc('\n');
    tfs_puts("bloco liberado = "); tfs_putu(old_block); tfs_putc('\n');
    tfs_puts("bloco reusado  = "); tfs_putu(new_block); tfs_putc('\n');
    tfs_puts("Reutilizacao de inode: "); tfs_puts(reuse_inode ? "PASS" : "FAIL"); tfs_putc('\n');
    tfs_puts("Reutilizacao de bloco: "); tfs_puts(reuse_block ? "PASS" : "FAIL"); tfs_putc('\n');

    tfs_puts("\n========== FIM DA DEMONSTRACAO ==========\n");
}
