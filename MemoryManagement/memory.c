/*
INTEGRANTES:
- 1107838827 - JULIAN ANDRES GOMEZ CABRERA
- 1107838996 - SAMUEL OROZCO VARELA
- 1144103598 - JULIANA PEREZ ROCHA 
*/


/* librerias */
#include <stdio.h>
#include <stdbool.h>
#include <string.h>

/* definicion de constantes */
#define MEMORY_SIZE 1024
#define PARTITIONS 4
#define MAX_BLOCKS 20
#define PAGE_SIZE    4096   /* Tamaño de página: 4 KB   */
#define NUM_PAGES      16   /* Páginas lógicas por proc */
#define NUM_FRAMES      8   /* Marcos físicos totales   */
#define OFFSET_BITS    12   
#define MAX_SEGMENTS   8      /* Máximo de segmentos por proceso */
#define MEM_SIZE   65536      /* Memoria física total: 64 KB    */
/* Permisos de segmento (bitmask) */
#define SEG_READ    0x01     
#define SEG_WRITE   0x02
#define SEG_EXEC    0x04

/* Estructura para representar una particion fija. */
typedef struct {
    int process_id;
    int size;
    int used;
    bool occupied;
} Partition;

/* Arreglo global de particiones fijas. */
Partition memory[PARTITIONS];

/* Estructura que representa un bloque para particionamiento dinamico. */
typedef struct {
    int start;
    int size;
    int process_id;
    bool free;
} Block;

/* Arreglo global de bloques dinamicos y su cantidad activa. */
Block blocks[MAX_BLOCKS];
int block_count = 1;

/* Estructura de entrada para la tabla de paginas. */
typedef struct {
    int frame_number;
    int present;
    int dirty;
    int referenced;
    int read_only;
} PageTableEntry;

PageTableEntry page_table[NUM_PAGES];
int frame_map[NUM_FRAMES];
int page_faults = 0;
int tlb_hits = 0;
int total_accesses_pag = 0;
int clock_hand = 0;

/* Estructura de entrada para la tabla de segmentos. */
typedef struct {
    unsigned long base;      /* Dir. física de inicio           */
    unsigned long limit;     /* Tamaño máximo en bytes          */
    int           valid;     /* 1 = segmento cargado y válido   */
    int           perms;     /* Bitmask: SEG_READ|WRITE|EXEC    */
    char          name[16];  /* Nombre descriptivo              */
} SegmentEntry;

SegmentEntry seg_table[MAX_SEGMENTS];
int          num_segments    = 0;
int          total_accesses_seg  = 0;
int          seg_faults      = 0;
int          prot_errors     = 0;



/*
 * Inicializa la memoria para particionamiento estatico.
 * Cada entrada queda libre y con tamano prefijado.
 */
void fija_initialize_mem(){
    int size[PARTITIONS] = {100, 200, 300, 424};
    for(int i=0; i<PARTITIONS; i++)
    {
        memory[i].process_id = -1;
        memory[i].size = size[i];
        memory[i].used = 0;
        memory[i].occupied = false;
    }
}

/*
 * Inicializa la memoria para particionamiento dinamico.
 * Se crea un unico bloque libre de tamano MEMORY_SIZE.
 */
void dinamica_initialize_mem() {
    block_count = 1;
    blocks[0].start = 0;
    blocks[0].size = MEMORY_SIZE;
    blocks[0].free = true;
    blocks[0].process_id = -1;
}

/* Muestra el estado de las particiones fijas. */
void fija_print_memory() {
    printf("\nEstado de memoria:\n");
    for (int i = 0; i < PARTITIONS; i++) {
        printf("Particion: %d | id: %d | Tamaño: %d | %s | Tamaño ocupado: %d\n",
               i,
               memory[i].process_id,
               memory[i].size,
               memory[i].occupied ? "Ocupado" : "Libre",
               memory[i].used);
    }
}

/* Muestra el estado de los bloques dinamicos. */
void dinamica_print_memory() {
    printf("\nEstado de memoria:\n");
    for (int i = 0; i < block_count; i++) {
        printf("Bloque %d | id: %d | Inicio: %d | Tamaño: %d | %s\n",
               i,
               blocks[i].process_id,
               blocks[i].start,
               blocks[i].size,
               blocks[i].free ? "Libre" : "Ocupado");
    }
}


/*
 * Asigna un proceso usando Best Fit en memoria estatica.
 * Busca la particion libre que mejor ajuste al tamano pedido.
 */
void fija_bestfit(int pid, int size) {
    int best = -1;
    int best_diff = MEMORY_SIZE;

    for (int i = 0; i < PARTITIONS; i++) {
        if (!memory[i].occupied && memory[i].size >= size) {
            int diff = memory[i].size - size;
            if (diff < best_diff) {
                best = i;
                best_diff = diff;
            }
        }
    }

    if (best == -1) {
        printf("No hay particion disponible\n");
        return;
    }

    memory[best].occupied = true;
    memory[best].process_id = pid;
    memory[best].used = size;

    printf("Asignado en particion %d (Best Fit)\n", best);
}

/*
 * Asigna un proceso usando Worst Fit en memoria estatica.
 * Selecciona la particion libre mas grande disponible.
 */
void fija_worstfit(int pid, int size) {
    int worst = -1;
    int worst_diff = -1;

    for (int i = 0; i < PARTITIONS; i++) {
        if (!memory[i].occupied && memory[i].size >= size) {
            int diff = memory[i].size - size;
            if (diff > worst_diff) {
                worst = i;
                worst_diff = diff;
            }
        }
    }

    if (worst == -1) {
        printf("No hay particion disponible\n");
        return;
    }

    memory[worst].occupied = true;
    memory[worst].process_id = pid;
    memory[worst].used = size;

    printf("Asignado en particion %d (Worst Fit)\n", worst);
}

/*
 * Asigna un proceso usando Best Fit en memoria dinamica.
 * Si sobra espacio en el bloque elegido, divide el bloque.
 */
void allocate_bestfit(int pid, int size) {

    //establezco los valores para comparar
    int best_index=-1;
    int best_index_size=MEMORY_SIZE;

    if (block_count==1){
        best_index=0;
    }
    else{
    for (int i = 0; i < block_count; i++) {
        if (blocks[i].free && blocks[i].size >= size) {
            //comparar el tamano del bloque
            if(blocks[i].size==size){
                best_index=i;
                break;
            }
            if(blocks[i].size<best_index_size){
                best_index= i;
                best_index_size=blocks[i].size;
            }}}}
    
    if(best_index==-1){
        printf("No hay espacio suficiente.\n");
        return;    }
    
    // Crear nuevobloque si sobra espacio
    if (blocks[best_index].size > size) {
        if (block_count >= MAX_BLOCKS) {
        printf("No se pueden crear más bloques.\n");
        return;}
        for (int j = block_count; j > best_index; j--)
        blocks[j] = blocks[j - 1];

        blocks[best_index + 1].start = blocks[best_index].start + size;
        blocks[best_index + 1].size = blocks[best_index].size - size;
        blocks[best_index + 1].free = true;
        blocks[best_index + 1].process_id = -1;

        blocks[best_index].size = size;
        block_count++;}

        blocks[best_index].free = false;
        blocks[best_index].process_id=pid;
    }


/*
 * Asigna un proceso usando Worst Fit en memoria dinamica.
 * Selecciona el hueco libre mas grande y lo divide si es necesario.
 */
void allocate_worstfit(int pid, int size) {
    //establezco los valores para comparar
    int worst_index=-1;
    int worst_index_size=-1;

    for (int i = 0; i < block_count; i++) {
        if (blocks[i].free && blocks[i].size >= size) {
            //comparar el tamano del bloque
            if(blocks[i].size>worst_index_size){
                worst_index= i;
                worst_index_size=blocks[i].size;
            }}}
    
    if(worst_index==-1){
        printf("No hay espacio suficiente.\n");
        return;    }
    
    // Crear nuevobloque si sobra espacio
    if (blocks[worst_index].size > size) {
        if (block_count >= MAX_BLOCKS) {
        printf("No se pueden crear más bloques.\n");
        return;}

        for (int j = block_count; j > worst_index; j--)
        blocks[j] = blocks[j - 1];

        blocks[worst_index + 1].start = blocks[worst_index].start + size;
        blocks[worst_index + 1].size = blocks[worst_index].size - size;
        blocks[worst_index + 1].free = true;
        blocks[worst_index + 1].process_id = -1;

        blocks[worst_index].size = size;
        block_count++;}

        blocks[worst_index].free = false;
        blocks[worst_index].process_id=pid;
    }

/*
 * Libera el bloque asociado al PID indicado.
 * Solo marca libre; no compacta automaticamente.
 */
void free_block(int pid) {
    for (int i = 0; i < block_count; i++) {
        if (blocks[i].process_id == pid) {
            blocks[i].free = true;
            blocks[i].process_id = -1;
            printf("Proceso %d liberado del bloque %d\n", pid, i);
            return;
        }
        }
        printf("Proceso no encontrado");
    }

/*Libera la particion asocionada al PID indicado para el particionamiento fijo*/
void fija_free(int pid) {
    for (int i = 0; i < PARTITIONS; i++) {
        if (memory[i].process_id == pid && memory[i].occupied) {
            memory[i].occupied = false;
            memory[i].process_id = -1;
            memory[i].used = 0;
            printf("Proceso %d liberado de la particion %d\n", pid, i);
            return;
        }
    }
    printf("Proceso no encontrado en memoria.\n");
}

/*
 * Compacta memoria dinamica moviendo bloques ocupados al inicio.
 * Al final deja un solo bloque libre consolidado.
 */
void compact_memory() {
    int new_start = 0;

    for (int i = 0; i < block_count; i++) {
        if (!blocks[i].free) {
            blocks[i].start = new_start;
            new_start += blocks[i].size;
        }
    }

    /* Crear bloque libre consolidado */
    blocks[0].start = 0;
    int index = 0;

    for (int i = 0; i < block_count; i++) {
        if (!blocks[i].free) {
            blocks[index++] = blocks[i];
        }
    }

    blocks[index].start = new_start;
    blocks[index].size = MEMORY_SIZE - new_start;
    blocks[index].free = true;
    blocks[index].process_id = -1;

    block_count = index + 1;

    printf("\nMemoria compactada.\n");
}

/* Inicializa tabla de paginas y mapa de marcos fisicos. */
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


/* Busca el primer marco libre disponible. */
int find_free_frame(void) {
    int i;
    for (i = 0; i < NUM_FRAMES; i++) {
        if (frame_map[i] == 0) return i;
    }
    return -1;
}


/*
 * Atiende un page fault:
 * - toma marco libre si existe,
 * - si no, aplica reemplazo Clock,
 * - actualiza bits y mapeos.
 */
void handle_page_fault(int page_num) {
    int frame;
    page_faults++;
    printf("[PAGE FAULT] Pagina %d no esta en RAM.\n", page_num);

    frame = find_free_frame();

    /* Implementación del algoritmo Clock (Segunda Oportunidad) */
    if (frame == -1) {
        int victim_page = -1;
        
        printf("[MEMORIA LLENA] Ejecutando algoritmo de reemplazo Clock...\n");
        
        /* Buscar una página víctima */
        while (victim_page == -1) {
            if (page_table[clock_hand].present) {
                if (page_table[clock_hand].referenced == 1) {
                    /* Segunda oportunidad: apagar el bit y avanzar */
                    page_table[clock_hand].referenced = 0;
                } else {
                    /* Bit de referencia en 0: Víctima encontrada */
                    victim_page = clock_hand;
                }
            }
            /* Avanzar el puntero del reloj circularmente */
            clock_hand = (clock_hand + 1) % NUM_PAGES;
        }

        /* Simular Swap-Out si la página estaba sucia (modificada) */
        if (page_table[victim_page].dirty == 1) {
            printf("[SWAP-OUT] Pagina %d modificada -> Escribiendo en disco...\n", victim_page);
        } else {
            printf("[SWAP-OUT] Pagina %d limpia -> Descartando sin escribir...\n", victim_page);
        }

        /* Recuperar el marco de la víctima y actualizar su entrada */
        frame = page_table[victim_page].frame_number;
        page_table[victim_page].present = 0;
        page_table[victim_page].frame_number = -1;
        frame_map[frame] = 0; 
    }

    /* Cargar la nueva página en el marco obtenido (libre o liberado) */
    frame_map[frame]                  = 1;
    page_table[page_num].frame_number = frame;
    page_table[page_num].present      = 1;
    page_table[page_num].referenced   = 1;
    page_table[page_num].dirty        = 0; /* Recién cargada, está limpia */
    printf("[PAGE-IN] Pagina %d -> Marco fisico %d.\n", page_num, frame);
}


/*
 * Traduce una direccion logica a fisica en paginacion.
 * write_access: 0 lectura, 1 escritura.
 * Retorna -1 si hay error de rango o proteccion.
 */
long translate_address(long logical_addr, int write_access) {
    int  page_num;
    int  offset;
    long physical_addr;

    total_accesses_pag++;

    /* Extraer número de página y desplazamiento mediante bitwise */
    page_num = (int)(logical_addr >> OFFSET_BITS);
    offset   = (int)(logical_addr & (PAGE_SIZE - 1));

    printf("\n[TRADUCCION] Logica: 0x%04lX -> Pag: %d, Offset: 0x%03X, Acceso: %s\n",
           logical_addr, page_num, offset, write_access ? "ESCRITURA" : "LECTURA");

    if (page_num < 0 || page_num >= NUM_PAGES) {
        printf("[ERROR] Numero de pagina %d fuera de rango.\n", page_num);
        return -1;
    }

    /*  Verificar protección de escritura */
    if (write_access == 1 && page_table[page_num].read_only == 1) {
        printf("[PROTECCION] Escritura denegada en pagina %d (Violacion de segmento: Solo Lectura).\n", page_num);
        return -1;
    }

    if (!page_table[page_num].present) {
        handle_page_fault(page_num);
    } else {
        tlb_hits++;
        printf("[TLB HIT] Pagina %d -> Marco %d\n",
               page_num, page_table[page_num].frame_number);
    }

    /*  Calcular dirección física y actualizar bits de control */
    physical_addr = (long)(page_table[page_num].frame_number * PAGE_SIZE) + offset;
    
    /* Si es una escritura exitosa, la página se ensucia */
    if (write_access == 1) {
        page_table[page_num].dirty = 1;
    }
    /* Todo acceso (lectura o escritura) enciende el bit de referencia */
    page_table[page_num].referenced = 1;

    printf("[RESULTADO] Dir. fisica: 0x%05lX\n", physical_addr);
    return physical_addr;
}


/* Imprime las entradas relevantes de la tabla de paginas. */
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

/* Muestra estadisticas acumuladas del modulo de paginacion. */
void print_stats(void) {
    printf("\n=== ESTADISTICAS ===\n");
    printf("Accesos totales : %d\n", total_accesses_pag);
    printf("Page faults     : %d (%.1f%%)\n", page_faults,
           total_accesses_pag ? 100.0 * page_faults / total_accesses_pag : 0.0);
    printf("TLB hits        : %d (%.1f%%)\n", tlb_hits,
           total_accesses_pag ? 100.0 * tlb_hits / total_accesses_pag : 0.0);
}

/* Menu interactivo para probar paginacion. */
void paginacion() {
    int option;
    long logical_addr;
    int write;

    printf("\n--- Memoria virtual: Paginacion ---\n");
    init_page_table();

    while (1) {
        printf("\n--- MENU PAGINACION ---\n");
        printf("1. Acceder a direccion logica\n");
        printf("2. Ver tabla de paginas\n");
        printf("3. Ver estadisticas\n");
        printf("0. Volver al menu principal\n");
        printf("Seleccione: ");
        scanf("%d", &option);

        switch (option) {

            case 1:
                printf("Ingrese direccion logica (numero): ");
                scanf("%ld", &logical_addr);

                printf("Tipo de acceso (0 = Lectura, 1 = Escritura): ");
                scanf("%d", &write);

                translate_address(logical_addr, write);
                break;

            case 2:
                print_page_table();
                break;

            case 3:
                print_stats();
                break;

            case 0:
                return;

            default:
                printf("Opcion invalida\n");
        }
    }
}

/* Inicializa tabla de segmentos y contador asociado. */
void init_seg_table(void) {
    int i;
    for (i = 0; i < MAX_SEGMENTS; i++) {
        seg_table[i].valid = 0;
        seg_table[i].base  = 0;
        seg_table[i].limit = 0;
        seg_table[i].perms = 0;
        seg_table[i].name[0] = '\0';
    }
    num_segments = 0;
}

/*
 * Agrega un nuevo segmento en la tabla.
 * Retorna indice de segmento o -1 en caso de error.
 */
int add_segment(unsigned long base, unsigned long limit, int perms, const char *name) {
    int idx;

    /* Validación básica: tabla llena */
    if (num_segments >= MAX_SEGMENTS) {
        printf("[ERROR] Tabla de segmentos llena.\n");
        return -1;
    }

    /* Resolución TODO (Parte 1): Validaciones físicas y lógicas */
    if (limit == 0 || (base + limit) > MEM_SIZE) {
        printf("[ERROR] Parámetros inválidos para segmento '%s' (Desbordamiento o Límite 0).\n", name);
        return -1;
    }

    idx = num_segments++;
    seg_table[idx].base  = base;
    seg_table[idx].limit = limit;
    seg_table[idx].perms = perms;
    seg_table[idx].valid = 1;
    
    /* Uso de strncpy para asegurar compatibilidad estricta con ANSI C en lugar de snprintf */
    strncpy(seg_table[idx].name, name, sizeof(seg_table[idx].name) - 1);
    seg_table[idx].name[sizeof(seg_table[idx].name) - 1] = '\0';

    printf("[SEG] Seg %d '%s': base=0x%05lX, lim=0x%05lX, perms=%c%c%c\n",
           idx, name, base, limit,
           (perms & SEG_READ)  ? 'R' : '-',
           (perms & SEG_WRITE) ? 'W' : '-',
           (perms & SEG_EXEC)  ? 'X' : '-');
    return idx;
}

/*
 * Traduce par (segmento, offset) a direccion fisica.
 * Valida existencia, limite y permisos del segmento.
 */
long translate_seg(int seg_num, unsigned long offset, int access) {
    long physical_addr;
    total_accesses_seg++;

    printf("\n[TRADUCCION] Seg=%d, Offset=0x%05lX, Acceso=%s\n",
           seg_num, offset,
           (access == SEG_READ)  ? "LECTURA"  :
           (access == SEG_WRITE) ? "ESCRITURA" : "EJECUCION");

    /* Verificar número de segmento lógico (Trap al SO si falla) */
    if (seg_num < 0 || seg_num >= num_segments || !seg_table[seg_num].valid) {
        printf("[SEG FAULT] Segmento %d no existe o es invalido.\n", seg_num);
        seg_faults++;
        return -1;
    }

    /* Resolución TODO (Parte 2a): Verificar límite del segmento (Hardware Limit Register) */
    if (offset > seg_table[seg_num].limit) {
        printf("[SEG FAULT] Offset 0x%lX > Limite 0x%lX en seg %d ('%s').\n",
               offset, seg_table[seg_num].limit,
               seg_num, seg_table[seg_num].name);
        seg_faults++;
        return -1;
    }

    /* Resolución TODO (Parte 2b): Verificar permisos de acceso (Hardware Protection) */
    if ((seg_table[seg_num].perms & access) == 0) {
        printf("[PROTECCION] Acceso denegado en segmento '%s'. Se requeria permiso %s.\n",
               seg_table[seg_num].name, 
               (access == SEG_READ) ? "R" : (access == SEG_WRITE) ? "W" : "X");
        prot_errors++;
        return -1;
    }

    /* Resolución TODO (Parte 2c): Calcular y retornar la dirección física final */
    physical_addr = (long)(seg_table[seg_num].base + offset);
    printf("[OK] Dir. fisica: 0x%05lX (Base: 0x%05lX + Offset: 0x%05lX)\n",
           physical_addr, seg_table[seg_num].base, offset);
    
    return physical_addr;
}

/* Imprime tabla de segmentos vigente. */
void print_seg_table(void) {
    int i;
    printf("\n+-----+----------+----------+----------+--------+\n");
    printf("| Seg | Nombre   | Base     | Limite   | Perms  |\n");
    printf("+-----+----------+----------+----------+--------+\n");
    for (i = 0; i < num_segments; i++) {
        if (seg_table[i].valid) {
            printf("|  %d  | %-8s | 0x%05lX  | 0x%05lX  |  %c%c%c   |\n",
                i, seg_table[i].name,
                seg_table[i].base, seg_table[i].limit,
                (seg_table[i].perms & SEG_READ)  ? 'R' : '-',
                (seg_table[i].perms & SEG_WRITE) ? 'W' : '-',
                (seg_table[i].perms & SEG_EXEC)  ? 'X' : '-');
        }
    }
    printf("+-----+----------+----------+----------+--------+\n");
}

/* Muestra estadisticas del modulo de segmentacion. */
void print_stats_segmentacion(void) {
    printf("\n=== ESTADISTICAS ===\n");
    printf("Accesos totales   : %d\n", total_accesses_seg);
    printf("Seg faults        : %d\n", seg_faults);
    printf("Errores proteccion: %d\n", prot_errors);
    printf("Accesos validos   : %d\n",
           total_accesses_seg - seg_faults - prot_errors);
}

/* Menu interactivo para probar segmentacion. */
void segmentacion() {
    int option;
    int seg_num;
    unsigned long offset, base, limit;
    int access_choice, perms;
    char name[16];

    printf("\n--- Memoria virtual: Segmentacion ---\n");
    init_seg_table();

    /* Pre-cargamos los segmentos base del laboratorio para facilitar las pruebas rápidas */
    printf("[INFO] Cargando segmentos por defecto...\n");
    add_segment(0x4000, 0x0FFF, SEG_READ | SEG_EXEC, "codigo");
    add_segment(0x1000, 0x07FF, SEG_READ | SEG_WRITE, "datos");
    add_segment(0x9000, 0x0FFF, SEG_READ | SEG_WRITE, "pila");

    while (1) {
        printf("\n--- MENU SEGMENTACION ---\n");
        printf("1. Acceder a direccion (Segmento + Offset)\n");
        printf("2. Crear nuevo segmento\n");
        printf("3. Ver tabla de segmentos\n");
        printf("4. Ver estadisticas\n");
        printf("0. Volver al menu principal\n");
        printf("Seleccione: ");
        scanf("%d", &option);

        switch (option) {
            case 1:
                printf("Ingrese numero de segmento: ");
                scanf("%d", &seg_num);
                
                /* Nota: %li permite al usuario ingresar números en base 10 o en Hex (ej. 0x0A00) */
                printf("Ingrese desplazamiento (offset): ");
                scanf("%li", &offset);

                printf("Tipo de acceso (1 = Lectura, 2 = Escritura, 4 = Ejecucion): ");
                scanf("%d", &access_choice);

                translate_seg(seg_num, offset, access_choice);
                break;

            case 2:
                printf("Ingrese direccion base fisica: ");
                scanf("%li", &base);
                
                printf("Ingrese tamano maximo (limite): ");
                scanf("%li", &limit);
                
                printf("Ingrese permisos sumando valores (1=R, 2=W, 4=X, ej. 3 para R/W): ");
                scanf("%d", &perms);
                
                printf("Ingrese nombre del segmento (sin espacios): ");
                scanf("%15s", name);
                
                add_segment(base, limit, perms, name);
                break;

            case 3:
                print_seg_table();
                break;

            case 4:
                print_stats_segmentacion(); 
                break;

            case 0:
                printf("Saliendo del modulo de segmentacion...\n");
                return;

            default:
                printf("Opcion invalida. Intente de nuevo.\n");
        }
    }
}

/*
 * Imprime menu principal con todos los metodos disponibles.
 */
void menu(void) {
	printf("\n========================================\n");
	printf(" Simulador de Gestion de Memoria (ANSI C)\n");
	printf("========================================\n");
	printf("1. Particionamiento estatico - Best Fit\n");
	printf("2. Particionamiento estatico - Worst Fit\n");
	printf("3. Particionamiento dinamico - Best Fit\n");
	printf("4. Particionamiento dinamico - Worst Fit\n");
	printf("5. Memoria virtual con paginacion\n");
	printf("6. Memoria virtual con segmentacion (bonus)\n");
    printf("7. Reiniciar memorias\n");
	printf("0. Salir\n");
}

/*Imprime el menu para hacer Particionamiento fijo, se usa el mismo para el best y worst, solo cambia la asignacion*/
void menu_fija(int tipo) {
    int opcion;
    int pid, size;

    while (1) {
        printf("\n--- MENU PARTICIONAMIENTO FIJO ---\n");
        printf("1. Asignar proceso\n");
        printf("2. Eliminar proceso\n");
        printf("3. Imprimir memoria\n");
        printf("4. Volver al menu principal\n");
        printf("Seleccione: ");
        scanf("%d", &opcion);

        switch (opcion) {

            case 1:
            /*solicita el ID del proceso y el tamaño a asignar*/
                printf("Ingrese ID del proceso: ");
                scanf("%d", &pid);

                printf("Ingrese tamaño: ");
                scanf("%d", &size);
            /*De acuerdo a la eleccion en el menu principal lo asigna como best o worst*/
                if (tipo == 1) {
                    fija_bestfit(pid, size);
                } else {
                    fija_worstfit(pid, size);
                }
                break;

            case 2:
             /*solicita el ID del proceso a eliminar*/
                printf("Ingrese ID del proceso a eliminar: ");
                scanf("%d", &pid);
                fija_free(pid);
                break;

            case 3:
            /*imprime el estado actual de la memoria*/
                fija_print_memory();
                break;

            case 4:
                return;

            default:
                printf("Opcion invalida\n");
        }
    }
}
/*Imprime el menu para hacer Particionamiento fijo, se usa el mismo para el best y worst, solo cambia la asignacion*/
void menu_dinamica(int tipo) {
    int opcion;
    int pid, size;

    while (1) {
        printf("\n--- MENU PARTICIONAMIENTO DINAMICO ---\n");
        printf("1. Asignar proceso\n");
        printf("2. Eliminar proceso\n");
        printf("3. Imprimir memoria\n");
        printf("4. Compactar memoria\n");
        printf("5. Volver al menu principal\n");
        printf("Seleccione: ");
        scanf("%d", &opcion);

        switch (opcion) {

            case 1:
            /*solicita el ID del proceso y el tamaño a asignar*/
                printf("Ingrese ID del proceso: ");
                scanf("%d", &pid);

                printf("Ingrese tamaño: ");
                scanf("%d", &size);
            /*De acuerdo a la eleccion en el menu principal lo asigna como best o worst*/
                if (tipo == 1) {
                    allocate_bestfit(pid, size);
                } else {
                    allocate_worstfit(pid, size);
                }
                break;

            case 2:
                printf("Ingrese ID del proceso a eliminar: ");
                scanf("%d", &pid);
                free_block(pid);
                break;

            case 3:
                dinamica_print_memory();
                break;

            case 4:
                compact_memory();
                break;
            
            case 5:
                return;

            default:
                printf("Opcion invalida\n");
        }
    }
}
/*
 * Punto de entrada del programa.
 * Inicializa memoria, recibe opciones de usuario y llama
 * al modulo seleccionado para pruebas.
 */
int main() {
    dinamica_initialize_mem();
    fija_initialize_mem();
    int option;
    int pid, size, n, liberar, pro;

    while (1) {
        
        menu();
        printf("Seleccione una opcion: ");
        scanf("%d", &option);

        switch (option) {
            case 1:
                fija_initialize_mem();
                menu_fija(1);
                break;
            case 2:
                fija_initialize_mem();
                menu_fija(2);
                break;
            case 3:
                dinamica_initialize_mem();
                menu_dinamica(1);
                break;
            case 4:
                dinamica_initialize_mem();
                menu_dinamica(2); 
                break;
            case 5:
                init_page_table();
                paginacion(); 
                break;
            case 6: 
                init_seg_table();
                segmentacion();
                break;
            case 7:
                printf("Reiniciando memorias.\n");
                dinamica_initialize_mem();
                fija_initialize_mem();
                break;
            case 0:
                printf("Saliendo del simulador.\n");
                return 0;
            default:
                printf("Opcion no valida. Intente de nuevo.\n");
        }
    }

    return 0;
}