/* kernel.c
   Nucleo principal del sistema operativo simulado.
   Coordina memory.c, processes.c y files.c.

   Integraciones por modulo:
   - memory.c  : init_memoria, allocate_bestfit/worstfit, fija_bestfit/worstfit,
                 free_block, fija_free, dinamica_print_memory, fija_print_memory,
                 add_segment (segmentacion al crear proceso),
                 translate_address (paginacion al abrir archivo),
                 print_page_table, print_seg_table, menu_memoria
   - processes.c: init_procesos, init_sistema_recursos, crear_proceso,
                  ciclo_vida_proceso, liberar_pcb, llamar_planificador_por_id,
                  cargar_cola, resetear_procesos, imprimir_estadisticas,
                  estado_a_texto
   - files.c   : init_filesystem, crear_archivo, abrir_archivo,
                 escribir_archivo, leer_archivo, cerrar_archivo,
                 eliminar_archivo, listar_archivos
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "memory.h"
#include "processes.h"
#include "files.h"

/* ── Utilidad: limpiar \n que deja fgets ────────────────────── */
#define LIMPIAR(s) (s)[strcspn((s), "\n")] = '\0'

/* ── Lectura segura de entero — limpia buffer completamente ─── */
int leer_entero() {
    int val = 0;
    char linea[16];
    if(fgets(linea, sizeof(linea), stdin))
        sscanf(linea, "%d", &val);
    return val;
}

/* ── Configuracion de sesion ─────────────────────────────────── */
#define MAX_PROC_KERNEL    5
#define MAX_FILES_POR_PROC 3

static int alg_memoria   = 1;  /* 1=dinamica bestfit  2=dinamica worstfit
                                   3=fija bestfit       4=fija worstfit    */
static int alg_procesos  = 1;  /* 1=FCFS 2=RR 3=SJF 4=Prioridad 5=CFS    */
static int alg_traduccion = 1; /* 1=paginacion  2=segmentacion  3=ambos   */
static int siguiente_pid = 100;

/* ── Tabla de procesos activos del kernel ────────────────────── */
typedef struct {
    PCB *pcb;
    int  pid;
    int  en_uso;
    char archivos_abiertos[MAX_FILES_POR_PROC][MAX_NAME];
    int  num_archivos;
    int  seg_codigo;   /* indice del segmento de codigo en seg_table */
    int  seg_datos;    /* indice del segmento de datos en seg_table  */
} ProcesoActivo;

static ProcesoActivo tabla[MAX_PROC_KERNEL];

/* ── Recursos del sistema para el banquero ───────────────────── */
static const int RECURSOS_SISTEMA[MAX_RECURSOS] = {10, 5, 7};

/* ── Direccion logica base simulada por proceso ──────────────── */
static long base_logica = 0x1000;

/* ════════════════════════════════════════════════════════════════
   FUNCIONES INTERNAS DE LA TABLA
   ════════════════════════════════════════════════════════════════ */

void init_tabla_procesos() {
    int i;
    for(i = 0; i < MAX_PROC_KERNEL; i++) {
        tabla[i].en_uso       = 0;
        tabla[i].pcb          = NULL;
        tabla[i].pid          = 0;
        tabla[i].num_archivos = 0;
        tabla[i].seg_codigo   = -1;
        tabla[i].seg_datos    = -1;
    }
}

int buscar_slot_libre() {
    int i;
    for(i = 0; i < MAX_PROC_KERNEL; i++)
        if(!tabla[i].en_uso) return i;
    return -1;
}

int buscar_proceso(int pid) {
    int i;
    for(i = 0; i < MAX_PROC_KERNEL; i++)
        if(tabla[i].en_uso && tabla[i].pid == pid) return i;
    return -1;
}

int registrar_archivo(int idx, char nombre[]) {
    if(tabla[idx].num_archivos >= MAX_FILES_POR_PROC) {
        printf("[KERNEL] PID=%d ya tiene %d archivos abiertos (limite).\n",
               tabla[idx].pid, MAX_FILES_POR_PROC);
        return 0;
    }
    strncpy(tabla[idx].archivos_abiertos[tabla[idx].num_archivos],
            nombre, MAX_NAME - 1);
    tabla[idx].num_archivos++;
    return 1;
}

void desregistrar_archivo(int idx, char nombre[]) {
    int i, j;
    for(i = 0; i < tabla[idx].num_archivos; i++) {
        if(strncmp(tabla[idx].archivos_abiertos[i], nombre, MAX_NAME) == 0) {
            for(j = i; j < tabla[idx].num_archivos - 1; j++)
                strncpy(tabla[idx].archivos_abiertos[j],
                        tabla[idx].archivos_abiertos[j+1], MAX_NAME);
            tabla[idx].num_archivos--;
            return;
        }
    }
}

/* ════════════════════════════════════════════════════════════════
   ASIGNACION DE MEMORIA SEGUN ALGORITMO DE SESION
   ════════════════════════════════════════════════════════════════ */

void asignar_memoria(int pid, int size) {
    switch(alg_memoria) {
        case 1:
            printf("[MEM] Dinamica Best Fit  — PID=%d size=%d\n", pid, size);
            allocate_bestfit(pid, size);
            break;
        case 2:
            printf("[MEM] Dinamica Worst Fit — PID=%d size=%d\n", pid, size);
            allocate_worstfit(pid, size);
            break;
        case 3:
            printf("[MEM] Fija Best Fit      — PID=%d size=%d\n", pid, size);
            fija_bestfit(pid, size);
            break;
        case 4:
            printf("[MEM] Fija Worst Fit     — PID=%d size=%d\n", pid, size);
            fija_worstfit(pid, size);
            break;
    }
}

void liberar_memoria(int pid) {
    if(alg_memoria == 1 || alg_memoria == 2)
        free_block(pid);
    else
        fija_free(pid);
}

/* ════════════════════════════════════════════════════════════════
   SEGMENTACION — agrega segmentos al crear proceso
   ════════════════════════════════════════════════════════════════ */

void agregar_segmentos_proceso(int slot, int rafaga) {
    /* Solo ejecutar si el mecanismo de sesion incluye segmentacion */
    if(alg_traduccion != 2 && alg_traduccion != 3) {
        printf("[SEG] Segmentacion no activa en esta sesion.\n");
        return;
    }

    unsigned long base_cod  = (unsigned long)base_logica;
    unsigned long limit_cod = (unsigned long)(rafaga * 4);
    unsigned long base_dat  = base_cod + limit_cod;
    unsigned long limit_dat = (unsigned long)(rafaga * 8);

    char seg_name[16];

    snprintf(seg_name, sizeof(seg_name), "cod_%d", tabla[slot].pid);
    int sc = add_segment(base_cod, limit_cod,
                         SEG_READ | SEG_EXEC, seg_name);

    snprintf(seg_name, sizeof(seg_name), "dat_%d", tabla[slot].pid);
    int sd = add_segment(base_dat, limit_dat,
                         SEG_READ | SEG_WRITE, seg_name);

    tabla[slot].seg_codigo = sc;
    tabla[slot].seg_datos  = sd;

    base_logica += (long)(limit_cod + limit_dat + 0x100);

    printf("[SEG] Segmento codigo: base=0x%lx limit=%lu perms=R+X\n",
           base_cod, limit_cod);
    printf("[SEG] Segmento datos:  base=0x%lx limit=%lu perms=R+W\n",
           base_dat, limit_dat);
}

/* ════════════════════════════════════════════════════════════════
   PAGINACION — traduce direccion logica al abrir archivo
   ════════════════════════════════════════════════════════════════ */

void mapear_archivo_en_paginas(char nombre[]) {
    /* Simulamos que el contenido del archivo ocupa MAX_CONTENT bytes
       y calculamos cuantas paginas necesita */
    int paginas = (MAX_CONTENT + PAGE_SIZE - 1) / PAGE_SIZE;
    if(paginas < 1) paginas = 1;

    printf("[PAG] Mapeando '%s' en tabla de paginas (%d pagina(s))...\n",
           nombre, paginas);

    /* Traducimos una direccion logica representativa */
    long dir_logica = (long)(base_logica & 0xFFFF);
    long dir_fisica = translate_address(dir_logica, 0);

    if(dir_fisica >= 0)
        printf("[PAG] Direccion logica 0x%04lx → fisica 0x%04lx\n",
               dir_logica, dir_fisica);
    else
        printf("[PAG] Page fault — pagina no presente (normal en primera carga)\n");
}

/* ════════════════════════════════════════════════════════════════
   TERMINAR PROCESO — cleanup garantizado
   ════════════════════════════════════════════════════════════════ */

void terminar_proceso_kernel(int idx) {
    int i;
    ProcesoActivo *p = &tabla[idx];

    printf("\n[KERNEL] Terminando proceso PID=%d (%s)...\n",
           p->pid, p->pcb ? p->pcb->nombre : "?");

    /* 1. Cerrar todos los archivos abiertos — evita lock leak */
    if(p->num_archivos > 0) {
        printf("[KERNEL] Cerrando %d archivo(s) abierto(s)...\n",
               p->num_archivos);
        for(i = 0; i < p->num_archivos; i++) {
            printf("[KERNEL]   -> cerrando '%s'\n",
                   p->archivos_abiertos[i]);
            cerrar_archivo(p->archivos_abiertos[i]);
        }
        p->num_archivos = 0;
    }

    /* 2. Liberar memoria del proceso */
    printf("[KERNEL] Liberando memoria PID=%d...\n", p->pid);
    liberar_memoria(p->pid);

    /* 3. Liberar PCB */
    if(p->pcb) {
        ciclo_vida_proceso(p->pcb, TERMINADO, 0);
        liberar_pcb(p->pcb);
        p->pcb = NULL;
    }

    /* 4. Liberar slot */
    p->en_uso       = 0;
    p->pid          = 0;
    p->seg_codigo   = -1;
    p->seg_datos    = -1;
    printf("[KERNEL] Proceso terminado correctamente.\n");
}

/* ════════════════════════════════════════════════════════════════
   CONFIGURACION INICIAL DE SESION
   ════════════════════════════════════════════════════════════════ */

void configurar_sesion() {
    printf("\n╔═══════════════════════════════════════╗\n");
    printf("║     MINI KERNEL - Sistema Operativo   ║\n");
    printf("║     Universidad Santiago de Cali      ║\n");
    printf("╚═══════════════════════════════════════╝\n");
    printf("\n--- Configuracion de sesion ---\n");

    printf("\nAlgoritmo de memoria:\n");
    printf("  1. Dinamica Best Fit\n");
    printf("  2. Dinamica Worst Fit\n");
    printf("  3. Fija Best Fit\n");
    printf("  4. Fija Worst Fit\n");
    printf("Seleccione [1-4]: ");
    alg_memoria = leer_entero();
    if(alg_memoria < 1 || alg_memoria > 4) {
        printf("Opcion invalida, usando Dinamica Best Fit.\n");
        alg_memoria = 1;
    }

    printf("\nAlgoritmo de planificacion:\n");
    printf("  1. FCFS\n");
    printf("  2. Round Robin (quantum=%d)\n", QUANTUM);
    printf("  3. SJF\n");
    printf("  4. Prioridad\n");
    printf("  5. CFS (Arbol Rojo-Negro)\n");
    printf("Seleccione [1-5]: ");
    alg_procesos = leer_entero();
    if(alg_procesos < 1 || alg_procesos > 5) {
        printf("Opcion invalida, usando FCFS.\n");
        alg_procesos = 1;
    }

    printf("\nMecanismo de traduccion de direcciones:\n");
    printf("  1. Paginacion\n");
    printf("  2. Segmentacion\n");
    printf("  3. Ambos\n");
    printf("Seleccione [1-3]: ");
    alg_traduccion = leer_entero();
    if(alg_traduccion < 1 || alg_traduccion > 3) {
        printf("Opcion invalida, usando Paginacion.\n");
        alg_traduccion = 1;
    }

    printf("\n[KERNEL] Sesion configurada.\n");
    printf("[KERNEL] Memoria    : %s\n",
           alg_memoria == 1 ? "Dinamica Best Fit"  :
           alg_memoria == 2 ? "Dinamica Worst Fit" :
           alg_memoria == 3 ? "Fija Best Fit"      : "Fija Worst Fit");
    printf("[KERNEL] Planificacion: %s\n",
           alg_procesos == 1 ? "FCFS"      :
           alg_procesos == 2 ? "Round Robin" :
           alg_procesos == 3 ? "SJF"       :
           alg_procesos == 4 ? "Prioridad" : "CFS");
    printf("[KERNEL] Traduccion : %s\n",
           alg_traduccion == 1 ? "Paginacion" :
           alg_traduccion == 2 ? "Segmentacion" : "Ambos");
}

/* ════════════════════════════════════════════════════════════════
   MENUS
   ════════════════════════════════════════════════════════════════ */

void mostrar_menu() {
    printf("\n==== Mini Kernel ====\n");
    printf("--- Procesos ---\n");
    printf(" 1. Crear proceso\n");
    printf(" 2. Listar procesos\n");
    printf(" 3. Terminar proceso\n");
    printf(" 4. Planificar procesos activos\n");
    printf("--- Archivos ---\n");
    printf(" 5. Crear archivo\n");
    printf(" 6. Listar archivos\n");
    printf(" 7. Abrir archivo  (proceso)\n");
    printf(" 8. Leer archivo\n");
    printf(" 9. Escribir archivo\n");
    printf("10. Cerrar archivo (proceso)\n");
    printf("11. Eliminar archivo\n");
    printf("--- Sistema ---\n");
    printf("12. Modulo de memoria completo\n");
    printf("13. Tabla de paginas\n");
    printf("14. Tabla de segmentos\n");
    printf("15. Estado global\n");
    printf(" 0. Apagar sistema\n");
    printf("Seleccione: ");
}

void listar_procesos_activos() {
    int i, hay = 0;
    printf("\n--- Procesos activos ---\n");
    printf("%-6s %-16s %-10s %-8s %-6s %-6s %s\n",
           "PID","Nombre","Estado","Archivos","Segs","Segd",
           "Archivos abiertos");
    printf("%-6s %-16s %-10s %-8s %-6s %-6s %s\n",
           "---","------","------","--------","----","----",
           "-----------------");
    for(i = 0; i < MAX_PROC_KERNEL; i++) {
        if(tabla[i].en_uso) {
            int j;
            printf("%-6d %-16s %-10s %-8d %-6s %-6s ",
                   tabla[i].pid,
                   tabla[i].pcb ? tabla[i].pcb->nombre : "?",
                   tabla[i].pcb ? estado_a_texto(tabla[i].pcb->estado) : "?",
                   tabla[i].num_archivos,
                   tabla[i].seg_codigo >= 0 ? "si" : "N/A",
                   tabla[i].seg_datos  >= 0 ? "si" : "N/A");
            for(j = 0; j < tabla[i].num_archivos; j++)
                printf("%s%s", tabla[i].archivos_abiertos[j],
                       j < tabla[i].num_archivos - 1 ? ", " : "");
            printf("\n");
            hay = 1;
        }
    }
    if(!hay) printf("  (ninguno)\n");
}

void mostrar_estado_global() {
    printf("\n========================================\n");
    printf("        ESTADO GLOBAL DEL SISTEMA\n");
    printf("========================================\n");
    printf("\n[PROCESOS]\n");
    listar_procesos_activos();
    printf("\n[MEMORIA]\n");
    if(alg_memoria == 1 || alg_memoria == 2)
        dinamica_print_memory();
    else
        fija_print_memory();
    printf("\n[PAGINACION]\n");
    print_page_table();
    printf("\n[SEGMENTACION]\n");
    print_seg_table();
    printf("\n[ARCHIVOS]\n");
    listar_archivos();
    printf("========================================\n");
}

/* ════════════════════════════════════════════════════════════════
   MAIN
   ════════════════════════════════════════════════════════════════ */

int main() {

    int  opcion, pid, idx;
    char nombre[MAX_NAME];
    char texto[MAX_CONTENT];
    char nombre_proc[32];
    int  rafaga, prioridad;
    int  recursos_max[MAX_RECURSOS];

    /* 1. Inicializar modulos */
    init_memoria();
    init_procesos();
    init_filesystem();
    init_tabla_procesos();
    init_sistema_recursos(RECURSOS_SISTEMA);

    /* 2. Configurar sesion */
    configurar_sesion();

    do {
        mostrar_menu();
        opcion = leer_entero();

        switch(opcion) {

            /* ══ 1. CREAR PROCESO ══ */
            case 1: {
                int slot = buscar_slot_libre();
                if(slot < 0) {
                    printf("[KERNEL] Sistema lleno, no hay slots.\n");
                    break;
                }

                printf("Nombre del proceso: ");
                fgets(nombre_proc, sizeof(nombre_proc), stdin);
                LIMPIAR(nombre_proc);

                /* Si fgets leyo solo el 
, el nombre quedo vacio */
                if(nombre_proc[0] == '\0') {
                    fgets(nombre_proc, sizeof(nombre_proc), stdin);
                    LIMPIAR(nombre_proc);
                }

                printf("Rafaga de CPU: ");
                rafaga = leer_entero();

                printf("Prioridad (menor=mayor): ");
                prioridad = leer_entero();

                printf("Recursos maximos (%d valores, uno por linea):\n",
                       MAX_RECURSOS);
                int r;
                for(r = 0; r < MAX_RECURSOS; r++) {
                    printf("  Recurso %d: ", r);
                    recursos_max[r] = leer_entero();
                }

                pid = siguiente_pid++;

                /* a) Crear PCB */
                PCB *p = crear_proceso(pid, nombre_proc, rafaga,
                                       0, prioridad, 0, recursos_max);
                if(!p) {
                    printf("[KERNEL] Error al crear proceso.\n");
                    break;
                }

                /* b) Asignar memoria segun algoritmo de sesion */
                asignar_memoria(pid, rafaga * 10);

                /* c) Registrar en tabla */
                tabla[slot].pcb          = p;
                tabla[slot].pid          = pid;
                tabla[slot].en_uso       = 1;
                tabla[slot].num_archivos = 0;

                /* d) Agregar segmentos de codigo y datos — segmentacion */
                agregar_segmentos_proceso(slot, rafaga);

                ciclo_vida_proceso(p, LISTO, 0);
                printf("[KERNEL] Proceso '%s' creado con PID=%d.\n",
                       nombre_proc, pid);
                printf("[KERNEL] Puede abrir archivos con la opcion 7.\n");
                printf("[KERNEL] Si no hay archivos, creelos con la opcion 5.\n");
                break;
            }

            /* ══ 2. LISTAR PROCESOS ══ */
            case 2:
                listar_procesos_activos();
                break;

            /* ══ 3. TERMINAR PROCESO ══ */
            case 3:
                listar_procesos_activos();
                printf("PID a terminar: ");
                pid = leer_entero();
                idx = buscar_proceso(pid);
                if(idx < 0) {
                    printf("[KERNEL] PID=%d no encontrado.\n", pid);
                    break;
                }
                terminar_proceso_kernel(idx);
                break;

            /* ══ 4. PLANIFICAR PROCESOS ACTIVOS ══ */
            case 4: {
                int i, n = 0;
                PCB *procs[MAX_PROC_KERNEL];
                ReadyQueue cola;

                for(i = 0; i < MAX_PROC_KERNEL; i++)
                    if(tabla[i].en_uso && tabla[i].pcb)
                        procs[n++] = tabla[i].pcb;

                if(n == 0) {
                    printf("[KERNEL] No hay procesos para planificar.\n");
                    break;
                }

                printf("[KERNEL] Planificando %d proceso(s) con algoritmo %d...\n",
                       n, alg_procesos);
                resetear_procesos(procs, n);
                cargar_cola(&cola, procs, n);
                llamar_planificador_por_id(alg_procesos, &cola, procs, n);
                imprimir_estadisticas(procs, n);
                break;
            }

            /* ══ 5. CREAR ARCHIVO ══ */
            case 5:
                printf("Nombre archivo: ");
                fgets(nombre, sizeof(nombre), stdin);
                LIMPIAR(nombre);
                crear_archivo(nombre);
                printf("[KERNEL] Para operar sobre el, cree un proceso (op.1)\n");
                printf("[KERNEL] y luego abralo con la opcion 7.\n");
                break;

            /* ══ 6. LISTAR ARCHIVOS ══ */
            case 6:
                listar_archivos();
                break;

            /* ══ 7. ABRIR ARCHIVO vinculado a proceso ══ */
            case 7: {
                /* Prerequisito: debe haber al menos un proceso activo */
                int hay_proc = 0, ii;
                for(ii = 0; ii < MAX_PROC_KERNEL; ii++)
                    if(tabla[ii].en_uso) { hay_proc = 1; break; }
                if(!hay_proc) {
                    printf("[KERNEL] No hay procesos activos.\n");
                    printf("[KERNEL] Cree un proceso primero con la opcion 1.\n");
                    break;
                }

                listar_procesos_activos();
                printf("PID del proceso que abre: ");
                pid = leer_entero();

                idx = buscar_proceso(pid);
                if(idx < 0) {
                    printf("[KERNEL] PID=%d no encontrado.\n", pid);
                    break;
                }

                /* Mostrar archivos disponibles (cerrados) */
                printf("\nArchivos disponibles para abrir:\n");
                listar_archivos();

                printf("Nombre archivo: ");
                fgets(nombre, sizeof(nombre), stdin);
                LIMPIAR(nombre);

                /* 1. Verificar estado del archivo */
                int estado = archivo_disponible(nombre);
                if(estado == 0) {
                    printf("[KERNEL] Archivo '%s' no existe.\n", nombre);
                    printf("[KERNEL] Creelo primero con la opcion 5.\n");
                    break;
                }
                if(estado == 2) {
                    printf("[KERNEL] '%s' esta bloqueado por otro proceso.\n",
                           nombre);
                    printf("[KERNEL] Espere a que ese proceso lo cierre (op.10).\n");
                    break;
                }

                /* 2. Verificar limite de archivos del proceso */
                if(!registrar_archivo(idx, nombre)) break;

                /* 3. Abrir en filesystem — toma el lock */
                abrir_archivo(nombre);

                /* 4. Lock adquirido — mapear en tabla de paginas */
                mapear_archivo_en_paginas(nombre);
                printf("[KERNEL] Archivo '%s' vinculado a PID=%d.\n",
                       nombre, pid);
                printf("[KERNEL] Ahora puede leer (op.8) o escribir (op.9).\n");
                break;
            }

            /* ══ 8. LEER ARCHIVO ══ */
            case 8: {
                /* Mostrar procesos y sus archivos abiertos como guia */
                int hay_abiertos = 0, ia;
                for(ia = 0; ia < MAX_PROC_KERNEL; ia++)
                    if(tabla[ia].en_uso && tabla[ia].num_archivos > 0)
                        { hay_abiertos = 1; break; }

                if(!hay_abiertos) {
                    printf("[KERNEL] No hay archivos abiertos por ningun proceso.\n");
                    printf("[KERNEL] Abra un archivo primero con la opcion 7.\n");
                    break;
                }

                listar_procesos_activos();
                printf("Nombre archivo a leer: ");
                fgets(nombre, sizeof(nombre), stdin);
                LIMPIAR(nombre);
                leer_archivo(nombre);
                break;
            }

            /* ══ 9. ESCRIBIR ARCHIVO ══ */
            case 9: {
                int hay_ab = 0, iw;
                for(iw = 0; iw < MAX_PROC_KERNEL; iw++)
                    if(tabla[iw].en_uso && tabla[iw].num_archivos > 0)
                        { hay_ab = 1; break; }

                if(!hay_ab) {
                    printf("[KERNEL] No hay archivos abiertos por ningun proceso.\n");
                    printf("[KERNEL] Abra un archivo primero con la opcion 7.\n");
                    break;
                }

                listar_procesos_activos();
                printf("Nombre archivo a escribir: ");
                fgets(nombre, sizeof(nombre), stdin);
                LIMPIAR(nombre);
                printf("Texto: ");
                fgets(texto, sizeof(texto), stdin);
                LIMPIAR(texto);
                escribir_archivo(nombre, texto);
                break;
            }

            /* ══ 10. CERRAR ARCHIVO vinculado a proceso ══ */
            case 10: {
                int hay_c = 0, ic;
                for(ic = 0; ic < MAX_PROC_KERNEL; ic++)
                    if(tabla[ic].en_uso && tabla[ic].num_archivos > 0)
                        { hay_c = 1; break; }

                if(!hay_c) {
                    printf("[KERNEL] No hay archivos abiertos para cerrar.\n");
                    break;
                }

                listar_procesos_activos();
                printf("PID del proceso: ");
                pid = leer_entero();

                idx = buscar_proceso(pid);
                if(idx < 0) {
                    printf("[KERNEL] PID=%d no encontrado.\n", pid);
                    break;
                }

                if(tabla[idx].num_archivos == 0) {
                    printf("[KERNEL] PID=%d no tiene archivos abiertos.\n", pid);
                    break;
                }

                /* Mostrar archivos abiertos de ese proceso */
                printf("Archivos abiertos por PID=%d:\n", pid);
                int jc;
                for(jc = 0; jc < tabla[idx].num_archivos; jc++)
                    printf("  %d. %s\n", jc+1,
                           tabla[idx].archivos_abiertos[jc]);

                printf("Nombre archivo a cerrar: ");
                fgets(nombre, sizeof(nombre), stdin);
                LIMPIAR(nombre);

                cerrar_archivo(nombre);
                desregistrar_archivo(idx, nombre);
                printf("[KERNEL] Archivo '%s' desvinculado de PID=%d.\n",
                       nombre, pid);
                break;
            }

            /* ══ 11. ELIMINAR ARCHIVO ══ */
            case 11:
                listar_archivos();
                printf("Nombre archivo a eliminar: ");
                fgets(nombre, sizeof(nombre), stdin);
                LIMPIAR(nombre);
                eliminar_archivo(nombre);
                break;

            /* ══ 12. MODULO MEMORIA COMPLETO ══ */
            case 12:
                menu_memoria();
                break;

            /* ══ 13. TABLA DE PAGINAS ══ */
            case 13:
                print_page_table();
                print_stats();
                break;

            /* ══ 14. TABLA DE SEGMENTOS ══ */
            case 14:
                print_seg_table();
                print_stats_segmentacion();
                break;

            /* ══ 15. ESTADO GLOBAL ══ */
            case 15:
                mostrar_estado_global();
                break;

            /* ══ 0. APAGAR ══ */
            case 0: {
                int i;
                printf("\n[KERNEL] Iniciando secuencia de apagado...\n");

                /* ── Estado ANTES del cleanup ── */
                printf("\n╔═══════════════════════════════════════════════╗\n");
                printf("║         ESTADO FINAL DEL SISTEMA              ║\n");
                printf("║         (antes de liberar recursos)           ║\n");
                printf("╚═══════════════════════════════════════════════╝\n");

                printf("\n[PROCESOS ACTIVOS]\n");
                listar_procesos_activos();

                printf("\n[MEMORIA]\n");
                if(alg_memoria == 1 || alg_memoria == 2)
                    dinamica_print_memory();
                else
                    fija_print_memory();

                if(alg_traduccion == 1 || alg_traduccion == 3) {
                    printf("\n[TABLA DE PAGINAS]\n");
                    print_page_table();
                    print_stats();
                }

                if(alg_traduccion == 2 || alg_traduccion == 3) {
                    printf("\n[TABLA DE SEGMENTOS]\n");
                    print_seg_table();
                    print_stats_segmentacion();
                }

                printf("\n[ARCHIVOS]\n");
                listar_archivos();

                /* ── Cleanup garantizado ── */
                printf("\n╔═══════════════════════════════════════════════╗\n");
                printf("║         LIBERANDO RECURSOS                    ║\n");
                printf("╚═══════════════════════════════════════════════╝\n");
                for(i = 0; i < MAX_PROC_KERNEL; i++)
                    if(tabla[i].en_uso)
                        terminar_proceso_kernel(i);

                /* ── Estado DESPUES del cleanup ── */
                printf("\n╔═══════════════════════════════════════════════╗\n");
                printf("║         SISTEMA TRAS LIBERACION               ║\n");
                printf("╚═══════════════════════════════════════════════╝\n");

                printf("\n[MEMORIA]\n");
                if(alg_memoria == 1 || alg_memoria == 2)
                    dinamica_print_memory();
                else
                    fija_print_memory();

                printf("\n[ARCHIVOS]\n");
                listar_archivos();

                printf("\n[KERNEL] Sistema apagado limpiamente.\n");
                break;
            }

            default:
                printf("Opcion invalida.\n");
        }

    } while(opcion != 0);

    return 0;
}
