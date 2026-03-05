//PAGINACIÓN Y SEGMENTACIÓN
/**
 * lab_paginacion.c
 * Laboratorio: Simulación de Memoria Virtual con Paginación
 *
 * Sistemas Operativos
 * Referencia: Stallings, Cap. 8 | Tanenbaum, Cap. 3.3
 *
 * Compilar: gcc -ansi -Wall -o lab_paginacion lab_paginacion.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Constantes del sistema simulado ───────────────────── */
#define PAGE_SIZE    4096   /* Tamaño de página: 4 KB   */
#define NUM_PAGES      16   /* Páginas lógicas por proc */
#define NUM_FRAMES      8   /* Marcos físicos totales   */
#define OFFSET_BITS    12   /* log2(PAGE_SIZE)          */

/* ── Entrada de la tabla de páginas ─────────────────────
 * Stallings, Fig. 8.8 | Tanenbaum, Fig. 3-11            */
typedef struct {
    int frame_number; /* Marco físico asignado (-1 si no presente) */
    int present;      /* 1 = en RAM, 0 = en swap                  */
    int dirty;        /* 1 = página modificada                     */
    int referenced;   /* 1 = accedida recientemente (Clock / NRU)  */
    int read_only;    /* 1 = solo lectura                          */
} PageTableEntry;

PageTableEntry page_table[NUM_PAGES];
int            frame_map[NUM_FRAMES]; /* 1=ocupado, 0=libre */

int total_accesses = 0;
int page_faults    = 0;
int tlb_hits       = 0;

/* ── Prototipos ─────────────────────────────────────────*/
void init_page_table(void);
int  find_free_frame(void);
void handle_page_fault(int page_num);
long translate_address(long logical_addr, int write_access);
void print_page_table(void);
void print_stats(void);

/* ════════════════════════════════════════════════════════
 * init_page_table: Todas las páginas sin asignar.        */
void init_page_table(void) {
    int i;
    for (i = 0; i < NUM_PAGES; i++) {
        page_table[i].frame_number = -1;
        page_table[i].present      =  0;
        page_table[i].dirty        =  0;
        page_table[i].referenced   =  0;
        page_table[i].read_only    =  0;
    }
    memset(frame_map, 0, sizeof(frame_map));
}

/* ════════════════════════════════════════════════════════
 * find_free_frame: Primer marco físico disponible.
 * Retorna: índice del marco, o -1 si no hay ninguno.     */
int find_free_frame(void) {
    int i;
    for (i = 0; i < NUM_FRAMES; i++) {
        if (frame_map[i] == 0) return i;
    }
    return -1;
}

/* ════════════════════════════════════════════════════════
 * handle_page_fault: Manejador simplificado de page fault.
 * En un SO real leería la página del disco.              */
void handle_page_fault(int page_num) {
    int frame;
    page_faults++;
    printf("[PAGE FAULT] Página %d no está en RAM.\n", page_num);

    frame = find_free_frame();

    if (frame == -1) {
        /* TODO (Parte 2): Implementar algoritmo de reemplazo de páginas.
         *
         * Pasos sugeridos:
         *   1. Recorrer la tabla de páginas buscando la víctima.
         *      (Usa el bit 'referenced' para Clock: elige la primera
         *       página con referenced == 0, poniendo en 0 las que tienen 1.)
         *   2. Si la víctima tiene dirty == 1, simula swap-out:
         *         printf("[SWAP-OUT] Página %d → disco\n", victim);
         *   3. Marca la víctima: present = 0, frame_number = -1.
         *   4. Libera su marco: frame_map[marco_victima] = 0.
         *   5. Asigna el marco liberado a page_num.                       */
        printf("[ERROR] Sin marcos libres. Implementar reemplazo (TODO Parte 2).\n");
        exit(1);
    }

    /* Cargar la página en el marco libre */
    frame_map[frame]                  = 1;
    page_table[page_num].frame_number = frame;
    page_table[page_num].present      = 1;
    page_table[page_num].referenced   = 1;
    page_table[page_num].dirty        = 0;
    printf("[PAGE-IN] Página %d → marco físico %d.\n", page_num, frame);
}

/* ════════════════════════════════════════════════════════
 * translate_address: Dirección lógica → física.
 *
 * Parámetros:
 *   logical_addr : dirección lógica de 16 bits
 *   write_access : 1=escritura, 0=lectura
 * Retorna: dirección física, o -1 si hay error.          */
long translate_address(long logical_addr, int write_access) {
    int  page_num;
    int  offset;
    long physical_addr;

    total_accesses++;

    /* TODO (Parte 1a): Extraer número de página y desplazamiento.
     *
     * Fórmulas:
     *   page_num = (int)(logical_addr >> OFFSET_BITS)
     *   offset   = (int)(logical_addr &  (PAGE_SIZE - 1))
     *
     * Reemplaza las dos líneas siguientes:                               */
    page_num = 0;  /* TODO: calcular correctamente */
    offset   = 0;  /* TODO: calcular correctamente */

    printf("\n[TRADUCCION] Lógica: 0x%04lX → Pág: %d, Offset: 0x%03X\n",
           logical_addr, page_num, offset);

    if (page_num < 0 || page_num >= NUM_PAGES) {
        printf("[ERROR] Número de página %d fuera de rango.\n", page_num);
        return -1;
    }

    /* TODO (Parte 1b): Verificar protección de escritura.
     *
     * Si write_access == 1 && page_table[page_num].read_only == 1:
     *   printf("[PROTECCION] Escritura denegada en página %d (solo lectura).\n", page_num);
     *   return -1;                                                        */

    if (!page_table[page_num].present) {
        handle_page_fault(page_num);
    } else {
        tlb_hits++;
        printf("[TLB HIT] Página %d → Marco %d\n",
               page_num, page_table[page_num].frame_number);
    }

    /* TODO (Parte 1c): Calcular dirección física y actualizar bits.
     *
     *   physical_addr = (long)page_table[page_num].frame_number
     *                   * PAGE_SIZE + offset;
     *   if (write_access) page_table[page_num].dirty = 1;
     *   page_table[page_num].referenced = 1;
     *   printf("[RESULTADO] Dir. física: 0x%05lX\n", physical_addr);
     *   return physical_addr;                                             */
    physical_addr = 0; /* TODO: reemplazar con el cálculo correcto */
    printf("[RESULTADO] Dir. física: 0x%05lX\n", physical_addr);
    return physical_addr;
}

/* ════════════════════════════════════════════════════════
 * print_page_table: Estado actual de la tabla de páginas.*/
void print_page_table(void) {
    int i;
    printf("\n+-------+--------+---------+-------+---------+\n");
    printf("| Pag.  | Marco  |Presente | Dirty | Ref.    |\n");
    printf("+-------+--------+---------+-------+---------+\n");
    for (i = 0; i < NUM_PAGES; i++) {
        if (page_table[i].present || page_table[i].frame_number != -1) {
            printf("|  %3d  |  %3d   |    %s    |   %s   |   %s     |\n",
                i,
                page_table[i].frame_number,
                page_table[i].present    ? "S" : "N",
                page_table[i].dirty      ? "S" : "N",
                page_table[i].referenced ? "S" : "N");
        }
    }
    printf("+-------+--------+---------+-------+---------+\n");
}

void print_stats(void) {
    printf("\n=== ESTADISTICAS ===\n");
    printf("Accesos totales : %d\n", total_accesses);
    printf("Page faults     : %d (%.1f%%)\n", page_faults,
           total_accesses ? 100.0 * page_faults / total_accesses : 0.0);
    printf("TLB hits        : %d (%.1f%%)\n", tlb_hits,
           total_accesses ? 100.0 * tlb_hits / total_accesses : 0.0);
}

/* ════════════════════════════════════════════════════════
 * MAIN — Casos de prueba                                  */
int main(void) {
    printf("=== Laboratorio: Paginacion - Sistemas Operativos ===\n");

    init_page_table();

    /* Marcar página 1 como solo lectura (segmento de código) */
    page_table[1].read_only = 1;

    /* ── Accesos de prueba ─────────────────────────────── */
    translate_address(0x3A4F, 0); /* Lectura: pág 3, offset 0xA4F  */
    translate_address(0x3100, 1); /* Escritura: pág 3 (ya en RAM)  */
    translate_address(0x2050, 0); /* Lectura: pág 2 (page fault)   */
    translate_address(0x1000, 1); /* Escritura pág 1 read-only→err */

    /* TODO (Parte 3): Agregar al menos 5 accesos adicionales:
     *   - Distintas páginas para provocar más page faults.
     *   - Al menos una escritura en página válida (observar dirty bit).
     *   - Acceso doble a la misma página para observar TLB hit.
     *
     * Ejemplo:
     *   translate_address(0x5ABC, 0);
     *   translate_address(0x5DEF, 1);
     *   ...                                                               */

    print_page_table();
    print_stats();
    return 0;
}
