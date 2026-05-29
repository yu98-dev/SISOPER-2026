/*
 * memory.h
 * Declaraciones publicas del modulo de gestion de memoria.
 *
 * INTEGRANTES:
 * - 1107838827 - JULIAN ANDRES GOMEZ CABRERA
 * - 1107838996 - SAMUEL OROZCO VARELA
 * - 1144103598 - JULIANA PEREZ ROCHA
 */

#ifndef MEMORY_H
#define MEMORY_H

#include <stdbool.h>

/* ── Constantes ─────────────────────────────────────────────── */
#define MEMORY_SIZE   1024
#define PARTITIONS       4
#define MAX_BLOCKS      20
#define PAGE_SIZE     4096
#define NUM_PAGES       16
#define NUM_FRAMES       8
#define OFFSET_BITS     12
#define MAX_SEGMENTS     8
#define MEM_SIZE     65536

/* Permisos de segmento (bitmask) */
#define SEG_READ   0x01
#define SEG_WRITE  0x02
#define SEG_EXEC   0x04

/* ── Estructuras ─────────────────────────────────────────────── */

/* Particion fija */
typedef struct {
    int  process_id;
    int  size;
    int  used;
    bool occupied;
} Partition;

/* Bloque de memoria dinamica */
typedef struct {
    int  start;
    int  size;
    int  process_id;
    bool free;
} Block;

/* Entrada de tabla de paginas */
typedef struct {
    int frame_number;
    int present;
    int dirty;
    int referenced;
    int read_only;
} PageTableEntry;

/* Entrada de tabla de segmentos */
typedef struct {
    unsigned long base;
    unsigned long limit;
    int           valid;
    int           perms;
    char          name[16];
} SegmentEntry;

/* ── Inicializacion ──────────────────────────────────────────── */
void fija_initialize_mem(void);
void dinamica_initialize_mem(void);
void init_page_table(void);
void init_seg_table(void);

/* ── Particionamiento fijo ───────────────────────────────────── */
void fija_bestfit(int pid, int size);
void fija_worstfit(int pid, int size);
void fija_free(int pid);
void fija_print_memory(void);

/* ── Particionamiento dinamico ───────────────────────────────── */
void allocate_bestfit(int pid, int size);
void allocate_worstfit(int pid, int size);
void free_block(int pid);
void compact_memory(void);
void dinamica_print_memory(void);

/* ── Paginacion ──────────────────────────────────────────────── */
long translate_address(long logical_addr, int write_access);
void print_page_table(void);
void print_stats(void);

/* ── Segmentacion ────────────────────────────────────────────── */
int  add_segment(unsigned long base, unsigned long limit,
                 int perms, const char *name);
long translate_seg(int seg_num, unsigned long offset, int access);
void print_seg_table(void);
void print_stats_segmentacion(void);

/*
 * menu_memoria()
 * Muestra el menu completo del modulo de memoria.
 * El kernel la invoca como una opcion de su menu principal.
 * Reemplaza al antiguo main() de memory.c.
 */
void menu_memoria(void);

/* Funcion de inicializacion global — el kernel la llama al arrancar */
void init_memoria(void);

#endif /* MEMORY_H */
