/*
 * processes.h
 * Declaraciones publicas del modulo de planificacion de procesos.
 *
 * Universidad Santiago de Cali, 2026A
 * Laboratorio #2 Sistemas Operativos
 */

#ifndef PROCESSES_H
#define PROCESSES_H

#include <stdbool.h>

/* ── Constantes ──────────────────────────────────────────────── */
#define MAX_PROCESOS   10
#define MAX_RECURSOS    3
#define NUM_REGISTROS   8
#define QUANTUM         2
#define NICE_0_WEIGHT 1024
#define RED   true
#define BLACK false

/* ── Tipos base ──────────────────────────────────────────────── */

typedef enum {
    NUEVO,
    LISTO,
    EJECUCION,
    BLOQUEADO,
    TERMINADO
} EstadoProceso;

typedef struct {
    int registros[NUM_REGISTROS];
    int program_counter;
    int stack_pointer;
} ImagenProceso;

/* Declaracion adelantada — el struct completo se define abajo */
typedef struct PCB PCB;

struct PCB {
    int pid;
    char nombre[32];
    int prioridad;
    int rafaga_total;
    int rafaga_restante;
    int tiempo_llegada;
    int tiempo_respuesta;
    int tiempo_retorno;
    int tiempo_espera;
    int ultimo_tiempo_en_cola;
    int contador_desalojos;
    EstadoProceso estado;

    int recursos_asignados[MAX_RECURSOS];
    int recursos_maximos[MAX_RECURSOS];
    int recursos_necesarios[MAX_RECURSOS];
    bool recursos_otorgados;

    long vruntime;
    int nice;
    int peso_cfs;

    ImagenProceso imagen;

    bool color;
    PCB *izq;
    PCB *der;
};

typedef struct {
    PCB *procesos[MAX_PROCESOS];
    int frente;
    int final;
    int cantidad;
} ReadyQueue;

typedef struct {
    PCB *raiz;
    int cantidad;
} ArbolRB;

typedef struct {
    int total[MAX_RECURSOS];
    int disponible[MAX_RECURSOS];
} EstadoSistema;

typedef struct {
    int total_procesos;
    int tiempo_total;
    int tiempo_espera_total;
    int tiempo_retorno_total;
    int cambios_contexto;
} EstadisticasScheduling;

/* ── Inicializacion ──────────────────────────────────────────── */

/*
 * init_procesos()
 * Inicializa el modulo de procesos.
 * El kernel la llama una sola vez al arrancar.
 */
void init_procesos(void);

/*
 * init_sistema_recursos()
 * Configura el vector de recursos totales del sistema.
 * Requerido por el algoritmo del banquero.
 */
void init_sistema_recursos(const int total[MAX_RECURSOS]);

/* ── Gestion de PCB ──────────────────────────────────────────── */
PCB *crear_proceso(int pid, const char *nombre, int rafaga,
                   int nice_val, int prioridad, int tiempo_llegada,
                   const int recursos_maximos[MAX_RECURSOS]);
void liberar_pcb(PCB *p);
void ciclo_vida_proceso(PCB *p, EstadoProceso nuevo_estado, int tiempo_actual);
void resetear_procesos(PCB *procesos[], int n);

/* ── Cola de listos ──────────────────────────────────────────── */
void  init_ready_queue(ReadyQueue *q);
bool  queue_vacia(const ReadyQueue *q);
bool  queue_llena(const ReadyQueue *q);
void  encolar(ReadyQueue *q, PCB *p, int tiempo_actual);
PCB  *desencolar(ReadyQueue *q);
void  cargar_cola(ReadyQueue *cola, PCB *procesos[], int n);

/* ── Planificadores ──────────────────────────────────────────── */
void planificar_fcfs(ReadyQueue *q, PCB *procesos[], int num_procesos);
void planificar_round_robin(ReadyQueue *q, int quantum,
                            PCB *procesos[], int num_procesos);
void planificar_sjf(ReadyQueue *q, PCB *procesos[], int num_procesos);
void planificar_prioridad(ReadyQueue *q, PCB *procesos[], int num_procesos);
void planificar_cfs(ReadyQueue *q, PCB *procesos[], int num_procesos);
void llamar_planificador_por_id(int id, ReadyQueue *cola,
                                PCB *procesos[], int num_procesos);

/* ── Estadisticas e impresion ────────────────────────────────── */
void reset_stats(void);
void imprimir_estadisticas(PCB *procesos[], int num_procesos);
const char *estado_a_texto(EstadoProceso e);

/*
 * menu_procesos()
 * Menu interactivo completo del modulo de planificacion.
 * El kernel lo invoca como submenu desde su menu principal.
 * Reemplaza al antiguo main() de processes.c.
 */
void menu_procesos(void);

#endif /* PROCESSES_H */
