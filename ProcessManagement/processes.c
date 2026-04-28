/*
 * processes.c - IMPLEMENTACIÓN COMPLETA
 * Universidad Santiago de Cali, 2026A
 * Laboratorio #2 Sistemas Operativos - Planificación y Representación de Procesos
 * 
 * Algoritmos: FCFS, Round Robin, SJF, Prioridad Simple, CFS con Árbol Rojo-Negro
 * Gestión de Bloqueos: Algoritmo del Banquero (Prevención) + Detección de Deadlocks
 * 
 * COMPILAR: gcc -Wall -std=c99 processes.c -o scheduler_lab
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>

#define MAX_PROCESOS 10
#define MAX_RECURSOS 3
#define QUANTUM 2
#define NICE_0_WEIGHT 1024

/* ==================== ESTRUCTURAS DE DATOS ==================== */

/* Forward declaration para referencias circulares */
typedef struct PCB PCB;

/* Estados del ciclo de vida del proceso */
typedef enum {
    NUEVO, LISTO, EJECUCION, BLOQUEADO, TERMINADO
} EstadoProceso;

/* Bloque de Control de Proceso (PCB) - Estructura fundamental */
struct PCB {
    int pid;                        /* Identificador único del proceso */
    char nombre[32];                /* Nombre descriptivo */
    int prioridad;                  /* Prioridad numérica (menor = más importante) */
    int rafaga_total;               /* Tiempo total de CPU necesario */
    int rafaga_restante;            /* Tiempo de CPU restante */
    int tiempo_llegada;             /* Tiempo en que entró al sistema */
    int tiempo_respuesta;           /* Estadística: primer acceso a CPU */
    int tiempo_retorno;             /* Estadística: salida - entrada */
    int tiempo_espera;              /* Estadística: retorno - rafaga_total */
    int ultimo_tiempo_en_cola;      /* Ultimo instante en que entro a la cola (round robin)*/
    EstadoProceso estado;           /* Estado actual del proceso */
    
    /* Recursos asignados (para Algoritmo del Banquero) */
    int recursos_asignados[MAX_RECURSOS];
    int recursos_maximos[MAX_RECURSOS];
    int recursos_necesarios[MAX_RECURSOS];  /* Máximo - Asignado */
    
    /* Parámetros de CFS (Completely Fair Scheduler) */
    long vruntime;                  /* Tiempo virtual ejecutado */
    int nice;                       /* Prioridad nice (-20..19) */
    int peso_cfs;                   /* Peso calculado de CFS */
    
    /* Contexto del proceso (simulado) */
    int *contexto_registros;        /* Simulación de registros guardados */
    int contador_desalojos;         /* Número de veces desalojado */
    
    /* Nodo para árbol Rojo-Negro */
    bool color;                     /* true=rojo, false=negro */
    PCB *izq, *der, *padre;         /* Punteros árbol RB */
};

/* Cola de procesos FIFO */
typedef struct {
    PCB* procesos[MAX_PROCESOS];
    int frente, final;
    int cantidad;
} ReadyQueue;

/* Árbol Rojo-Negro para CFS */
typedef struct {
    PCB *raiz;
    int cantidad;
} ArbolRB;

/* Helpers de color */
static inline bool es_rojo(PCB *n) {
    return (n != NULL && n->color == true);
}

static inline bool es_negro(PCB *n) {
    return (n == NULL || n->color == false);
}

/* Matriz de Recursos para el Algoritmo del Banquero */
typedef struct {
    int total[MAX_RECURSOS];                    /* Total de recursos en sistema */
    int disponible[MAX_RECURSOS];               /* Recursos disponibles */
    int max[MAX_PROCESOS][MAX_RECURSOS];        /* Máximo requerido por proceso */
    int asignado[MAX_PROCESOS][MAX_RECURSOS];   /* Asignado a cada proceso */
    int necesario[MAX_PROCESOS][MAX_RECURSOS];  /* Aún necesita cada proceso */
} EstadoSistema;

/* Estructura para estadísticas de planificación */
typedef struct {
    int total_procesos;
    int tiempo_total;
    int tiempo_espera_total;
    int tiempo_retorno_total;
    float tiempo_respuesta_promedio;
    float utilizacion_cpu;
    int cambios_contexto;
} EstadisticasScheduling;

/* ==================== VARIABLES GLOBALES ==================== */
EstadoSistema sistema_recursos;
EstadisticasScheduling stats;

/* ==================== FUNCIONES AUXILIARES DE COLAS ==================== */

void init_ready_queue(ReadyQueue *q) {
    q->frente = 0;
    q->final = 0;
    q->cantidad = 0;
    memset(q->procesos, 0, sizeof(q->procesos));
}

bool queue_vacia(ReadyQueue *q) {
    return q->cantidad == 0;
}

bool queue_llena(ReadyQueue *q) {
    return q->cantidad == MAX_PROCESOS;
}

void encolar(ReadyQueue *q, PCB *p, int tiempo_actual) {
    if (queue_llena(q)) {
        fprintf(stderr, "Error: Cola llena\n");
        return;
    }
    q->procesos[q->final] = p;
    q->final = (q->final + 1) % MAX_PROCESOS;
    q->cantidad++;
    p->estado = LISTO;
    p->ultimo_tiempo_en_cola = tiempo_actual;
}

PCB* desencolar(ReadyQueue *q) {
    if (queue_vacia(q)) return NULL;
    PCB *p = q->procesos[q->frente];
    q->frente = (q->frente + 1) % MAX_PROCESOS;
    q->cantidad--;
    return p;
}

/* ==================== FUNCIONES AUXILIARES PCB ==================== */

PCB* crear_proceso(int pid, const char *nombre, int rafaga, int nice_val,
                   int prioridad, int tiempo_llegada,
                   int *max_recursos, int *necesarios) {
    PCB *nuevo = (PCB*)malloc(sizeof(PCB));
    nuevo->pid = pid;
    strcpy(nuevo->nombre, nombre);
    nuevo->rafaga_total = rafaga;
    nuevo->rafaga_restante = rafaga;
    nuevo->prioridad = prioridad;
    nuevo->nice = nice_val;
    nuevo->vruntime = 0;
    nuevo->estado = NUEVO;
    nuevo->tiempo_llegada = tiempo_llegada;
    nuevo->tiempo_respuesta = -1;
    nuevo->tiempo_retorno = 0;
    nuevo->tiempo_espera = 0;
    nuevo->contador_desalojos = 0;
    nuevo->contexto_registros = NULL;
    
    /* Recursos */
    for (int i = 0; i < MAX_RECURSOS; i++) {
        nuevo->recursos_asignados[i] = 0;
        nuevo->recursos_maximos[i] = max_recursos[i];
        nuevo->recursos_necesarios[i] = necesarios[i];
    }
    
    /* CFS */
    nuevo->peso_cfs = NICE_0_WEIGHT;
    
    /* Árbol RB */
    nuevo->izq = nuevo->der = nuevo->padre = NULL;
    nuevo->color = true;  /* Rojo por defecto */
    
    return nuevo;
}

void liberar_pcb(PCB *p) {
    if (p->contexto_registros) free(p->contexto_registros);
    free(p);
}

/* ==================== GESTIÓN DEL CICLO DE VIDA DEL PROCESO ==================== */

/**
 * Transición de estados del proceso
 * Modela el diagrama de estados: NUEVO -> LISTO -> EJECUCION -> (BLOQUEADO/TERMINADO)
 */
void cambiar_estado(PCB *p, EstadoProceso nuevo_estado, int tiempo_actual) {
    EstadoProceso anterior = p->estado;
    
    switch (anterior) {
        case NUEVO:
            if (nuevo_estado == LISTO) p->estado = LISTO;
            break;
        case LISTO:
            if (nuevo_estado == EJECUCION) {
                p->estado = EJECUCION;
                if (p->tiempo_respuesta == -1)
                    p->tiempo_respuesta = tiempo_actual - p->tiempo_llegada;
            }
            break;
        case EJECUCION:
            if (nuevo_estado == TERMINADO) {
                p->estado = TERMINADO;
                p->tiempo_retorno = tiempo_actual - p->tiempo_llegada;
                p->tiempo_espera = p->tiempo_retorno - p->rafaga_total;
            } else if (nuevo_estado == BLOQUEADO) {
                p->estado = BLOQUEADO;
            } else if (nuevo_estado == LISTO) {
                p->estado = LISTO;
                p->contador_desalojos++;
            }
            break;
        case BLOQUEADO:
            if (nuevo_estado == LISTO) p->estado = LISTO;
            break;
        case TERMINADO:
            /* No hay transiciones desde TERMINADO */
            break;
    }
}

/**
 * Calcula peso de CFS basado en el valor nice
 * Fórmula simplificada: peso = NICE_0_WEIGHT / (1 + nice)
 */
int calcular_peso_cfs(int nice) {
    int peso_tabla[] = {88761, 71755, 56483, 46273, 36291, 29154, 23254, 18705,
                        14949, 11916, 9548, 7620, 6100, 4904, 3906, 3121,
                        2501, 1922, 1524, 1277, 1024};
    if (nice < -20) nice = -20;
    if (nice > 19) nice = 19;
    return peso_tabla[nice + 20];
}

/* ==================== ALGORITMO DEL BANQUERO (PREVENCIÓN DE DEADLOCKS) ==================== */

/**
 * Inicializa la matriz de estado del sistema
 */
void init_sistema_recursos(int *total_recursos) {
    for (int i = 0; i < MAX_RECURSOS; i++) {
        sistema_recursos.total[i] = total_recursos[i];
        sistema_recursos.disponible[i] = total_recursos[i];
    }
    memset(sistema_recursos.max, 0, sizeof(sistema_recursos.max));
    memset(sistema_recursos.asignado, 0, sizeof(sistema_recursos.asignado));
    memset(sistema_recursos.necesario, 0, sizeof(sistema_recursos.necesario));
}

/**
 * Verifica si una asignación es segura usando el algoritmo del banquero
 * Retorna: 1 si es segura, 0 si no
 */
int es_estado_seguro(PCB *procesos[], int num_procesos) {
    int necesario[MAX_PROCESOS][MAX_RECURSOS];
    int trabajo[MAX_RECURSOS];
    bool terminado[MAX_PROCESOS];
    int contador = 0;
    
    /* Copiar matriz de necesarios */
    for (int i = 0; i < num_procesos; i++) {
        for (int j = 0; j < MAX_RECURSOS; j++) {
            necesario[i][j] = procesos[i]->recursos_necesarios[j];
        }
        terminado[i] = (procesos[i]->estado == TERMINADO);
    }
    
    /* Copiar disponibles */
    for (int j = 0; j < MAX_RECURSOS; j++) {
        trabajo[j] = sistema_recursos.disponible[j];
    }
    
    /* Algoritmo del banquero */
    for (int i = 0; i < num_procesos; i++) {
        int encontrado = 0;
        for (int j = 0; j < num_procesos; j++) {
            if (!terminado[j]) {
                int puede_asignar = 1;
                for (int k = 0; k < MAX_RECURSOS; k++) {
                    if (necesario[j][k] > trabajo[k]) {
                        puede_asignar = 0;
                        break;
                    }
                }
                
                if (puede_asignar) {
                    /* Otorgar recursos y simular liberación */
                    for (int k = 0; k < MAX_RECURSOS; k++) {
                        trabajo[k] += procesos[j]->recursos_asignados[k];
                    }
                    contador++;
                    terminado[j] = true;
                    encontrado = 1;
                    break;
                }
            }
        }
        if (!encontrado) return 0;  /* Deadlock potencial */
    }
    
    return 1;  /* Estado seguro */
}

/**
 * Intenta asignar recursos a un proceso (Algoritmo del Banquero)
 */
int asignar_recursos_banquero(PCB *p, int *recursos_solicitados, PCB *procesos[], int num_procesos) {
    /* Verificar disponibilidad */
    for (int i = 0; i < MAX_RECURSOS; i++) {
        if (recursos_solicitados[i] > sistema_recursos.disponible[i])
            return 0;  /* No hay suficientes recursos */
    }
    
    /* Asignar provisionalmente */
    for (int i = 0; i < MAX_RECURSOS; i++) {
        sistema_recursos.disponible[i] -= recursos_solicitados[i];
        p->recursos_asignados[i] += recursos_solicitados[i];
        p->recursos_necesarios[i] -= recursos_solicitados[i];
    }

        if (!es_estado_seguro(procesos, num_procesos)) {
        // rollback
        for (int i = 0; i < MAX_RECURSOS; i++) {
            sistema_recursos.disponible[i] += recursos_solicitados[i];
            p->recursos_asignados[i] -= recursos_solicitados[i];
            p->recursos_necesarios[i] += recursos_solicitados[i];
        }
        return 0;
    }
    
    return 1;  /* Asignación exitosa */
}

/* ==================== DETECCIÓN DE DEADLOCKS ==================== */

/**
 * Detecta ciclos en grafo de asignación de recursos
 */
int detectar_deadlock(PCB *procesos[], int num_procesos) {
    int trabajo[MAX_RECURSOS];
    bool procesado[MAX_PROCESOS];
    int cambios;
    
    /* Inicializar trabajo con disponibles */
    for (int i = 0; i < MAX_RECURSOS; i++) {
        trabajo[i] = sistema_recursos.disponible[i];
    }
    memset(procesado, false, sizeof(procesado));
    
    /* Iteración: marcar procesos que pueden terminar */
    do {
        cambios = 0;
        for (int i = 0; i < num_procesos; i++) {
            if (!procesado[i]) {
                int puede_terminar = 1;
                for (int j = 0; j < MAX_RECURSOS; j++) {
                    if (procesos[i]->recursos_necesarios[j] > trabajo[j]) {
                        puede_terminar = 0;
                        break;
                    }
                }
                
                if (puede_terminar) {
                    for (int j = 0; j < MAX_RECURSOS; j++) {
                        trabajo[j] += procesos[i]->recursos_asignados[j];
                    }
                    procesado[i] = true;
                    cambios = 1;
                }
            }
        }
    } while (cambios);
    
    /* Si hay procesos no procesados, hay deadlock */
    for (int i = 0; i < num_procesos; i++) {
        if (!procesado[i]) return 1;
    }
    return 0;
}

/**
 * Recuperación de deadlock: abortar proceso de menor prioridad
 */
void recuperar_deadlock(PCB *procesos[], int num_procesos) {
    int victima = -1;
    int min_prioridad = INT_MIN;
    
    for (int i = 0; i < num_procesos; i++) {
        if (procesos[i]->estado == BLOQUEADO && procesos[i]->prioridad < min_prioridad) {
            victima = i;
            min_prioridad = procesos[i]->prioridad;
        }
    }
    
    if (victima != -1) {
        printf("Deadlock detectado: Abortando proceso %s (PID %d)\n",
               procesos[victima]->nombre, procesos[victima]->pid);
        
        /* Liberar recursos */
        for (int j = 0; j < MAX_RECURSOS; j++) {
            sistema_recursos.disponible[j] += procesos[victima]->recursos_asignados[j];
            procesos[victima]->recursos_asignados[j] = 0;
        }
        procesos[victima]->estado = TERMINADO;
    }
}

/* ==================== ÁRBOL ROJO-NEGRO PARA CFS ==================== */

/**
 * Rotación izquierda en árbol RB
 */
void rotacion_izquierda(ArbolRB *arbol, PCB *x) {
    PCB *y = x->der;
    x->der = y->izq;
    if (y->izq != NULL) y->izq->padre = x;
    
    y->padre = x->padre;
    if (x->padre == NULL) {
        arbol->raiz = y;
    } else if (x == x->padre->izq) {
        x->padre->izq = y;
    } else {
        x->padre->der = y;
    }
    y->izq = x;
    x->padre = y;
}

/**
 * Rotación derecha en árbol RB
 */
void rotacion_derecha(ArbolRB *arbol, PCB *y) {
    PCB *x = y->izq;
    y->izq = x->der;
    if (x->der != NULL) x->der->padre = y;
    
    x->padre = y->padre;
    if (y->padre == NULL) {
        arbol->raiz = x;
    } else if (y == y->padre->der) {
        y->padre->der = x;
    } else {
        y->padre->izq = x;
    }
    x->der = y;
    y->padre = x;
}

/*arregla y colorea el arbol en cada insercion*/
void insert_fixup(ArbolRB *arbol, PCB *z) {
    while (z->padre && es_rojo(z->padre)) {
        PCB *p = z->padre;
        PCB *g = p->padre;

        if (p == g->izq) {
            PCB *y = g->der; // tío

            // Caso 1: tío rojo
            if (es_rojo(y)) {
                p->color = false;
                y->color = false;
                g->color = true;
                z = g;
            } else {
                // Caso 2: z es hijo derecho
                if (z == p->der) {
                    z = p;
                    rotacion_izquierda(arbol, z);
                    p = z->padre;
                    g = p->padre;
                }
                // Caso 3
                p->color = false;
                g->color = true;
                rotacion_derecha(arbol, g);
            }
        } else {
            // Simétrico
            PCB *y = g->izq;

            if (es_rojo(y)) {
                p->color = false;
                y->color = false;
                g->color = true;
                z = g;
            } else {
                if (z == p->izq) {
                    z = p;
                    rotacion_derecha(arbol, z);
                    p = z->padre;
                    g = p->padre;
                }
                p->color = false;
                g->color = true;
                rotacion_izquierda(arbol, g);
            }
        }
    }
    arbol->raiz->color = false; // raíz siempre negra
}

/**
 * Inserta un proceso en el árbol RB (ordenado por vruntime)
 */
void insertar_rb(ArbolRB *arbol, PCB *nuevo) {
    PCB *x = arbol->raiz;
    PCB *padre = NULL;
    
    /* Búsqueda de posición */
    while (x != NULL) {
        padre = x;
        if (nuevo->vruntime < x->vruntime) {
            x = x->izq;
        } else {
            x = x->der;
        }
    }
    
    nuevo->padre = padre;
    nuevo->izq = nuevo->der = NULL;
    nuevo->color = true;  /* Rojo */
    
    if (padre == NULL) {
        arbol->raiz = nuevo;
    } else if (nuevo->vruntime < padre->vruntime) {
        padre->izq = nuevo;
    } else {
        padre->der = nuevo;
    }
    
    arbol->cantidad++;
    insert_fixup(arbol, nuevo);
}

/**
 * Obtiene el nodo con vruntime mínimo
 */
PCB* obtener_minimo(ArbolRB *arbol) {
    if (arbol->raiz == NULL) return NULL;
    PCB *actual = arbol->raiz;
    while (actual->izq != NULL) {
        actual = actual->izq;
    }
    return actual;
}

/**
 * Elimina un proceso del árbol RB
 */

 void transplant(ArbolRB *arbol, PCB *u, PCB *v) {
    if (u->padre == NULL) {
        arbol->raiz = v;
    } else if (u == u->padre->izq) {
        u->padre->izq = v;
    } else {
        u->padre->der = v;
    }
    if (v != NULL) v->padre = u->padre;
}

PCB* minimo(PCB *n) {
    while (n && n->izq) n = n->izq;
    return n;
}

void delete_fixup(ArbolRB *arbol, PCB *x) {

    while (x != arbol->raiz && es_negro(x)) {
        if (x == x->padre->izq) {
            PCB *w = x->padre->der;

            if (es_rojo(w)) {
                w->color = false;
                x->padre->color = true;
                rotacion_izquierda(arbol, x->padre);
                w = x->padre->der;
            }

            if (es_negro(w->izq) && es_negro(w->der)) {
                w->color = true;
                x = x->padre;
            } else {
                if (es_negro(w->der)) {
                    if (w->izq) w->izq->color = false;
                    w->color = true;
                    rotacion_derecha(arbol, w);
                    w = x->padre->der;
                }

                w->color = x->padre->color;
                x->padre->color = false;
                if (w->der) w->der->color = false;
                rotacion_izquierda(arbol, x->padre);
                x = arbol->raiz;
            }
        } else {
            // Simétrico
            PCB *w = x->padre->izq;

            if (es_rojo(w)) {
                w->color = false;
                x->padre->color = true;
                rotacion_derecha(arbol, x->padre);
                w = x->padre->izq;
            }

            if (es_negro(w->der) && es_negro(w->izq)) {
                w->color = true;
                x = x->padre;
            } else {
                if (es_negro(w->izq)) {
                    if (w->der) w->der->color = false;
                    w->color = true;
                    rotacion_izquierda(arbol, w);
                    w = x->padre->izq;
                }

                w->color = x->padre->color;
                x->padre->color = false;
                if (w->izq) w->izq->color = false;
                rotacion_derecha(arbol, x->padre);
                x = arbol->raiz;
            }
        }
    }
    if (x) x->color = false;
}

void eliminar_rb(ArbolRB *arbol, PCB *z) {
    PCB *y = z;
    PCB *x = NULL;
    bool y_color_original = y->color;

    if (z->izq == NULL) {
        x = z->der;
        transplant(arbol, z, z->der);
    } else if (z->der == NULL) {
        x = z->izq;
        transplant(arbol, z, z->izq);
    } else {
        y = minimo(z->der); // sucesor
        y_color_original = y->color;
        x = y->der;

        if (y->padre == z) {
            if (x) x->padre = y;
        } else {
            transplant(arbol, y, y->der);
            y->der = z->der;
            y->der->padre = y;
        }

        transplant(arbol, z, y);
        y->izq = z->izq;
        y->izq->padre = y;
        y->color = z->color;
    }

    if (y_color_original == false && x != NULL) { // si era negro
        delete_fixup(arbol, x);
    }

    arbol->cantidad--;
}

/* ==================== ALGORITMOS DE PLANIFICACIÓN ==================== */

/**
 * FCFS (First Come First Served) - No preventivo
 * Estrategia de bloqueos: PREVENCIÓN por orden FIFO
 */
void planificar_fcfs(ReadyQueue *q, PCB *procesos[], int num_procesos) {
    printf("\n╔════════════════════════════════════════════════════╗\n");
    printf("║        ALGORITMO: FCFS (First Come First Served)   ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");
    printf("Estrategia de bloqueos: PREVENCIÓN (orden FIFO evita inanición)\n\n");
    
    ReadyQueue cola_copia;
    init_ready_queue(&cola_copia);
    while (!queue_vacia(q)) {
        encolar(&cola_copia, desencolar(q), 0);
    }
    
    int tiempo_actual = 0;
    stats.cambios_contexto = 0;
    
    while (!queue_vacia(&cola_copia)) {
        PCB *proceso = desencolar(&cola_copia);
        cambiar_estado(proceso, EJECUCION, tiempo_actual);
        stats.cambios_contexto++;
        
        printf("[t=%d] Ejecutando: %s (PID %d) | Ráfaga: %d\n",
               tiempo_actual, proceso->nombre, proceso->pid, proceso->rafaga_total);
        
        tiempo_actual += proceso->rafaga_total;
        cambiar_estado(proceso, TERMINADO, tiempo_actual);
        
        printf("       → Terminado | Tiempo retorno: %d | Espera: %d\n",
               proceso->tiempo_retorno, proceso->tiempo_espera);
        
        stats.tiempo_retorno_total += proceso->tiempo_retorno;
        stats.tiempo_espera_total += proceso->tiempo_espera;
    }
    
    stats.tiempo_total = tiempo_actual;
    stats.total_procesos = num_procesos;
}

/**
 * ROUND ROBIN - Preventivo con quantum fijo
 * Estrategia de bloqueos: PREVENCIÓN por equidad temporal
 */
void planificar_round_robin(ReadyQueue *q, int quantum, PCB *procesos[], int num_procesos) {
    printf("\n╔════════════════════════════════════════════════════╗\n");
    printf("║    ALGORITMO: ROUND ROBIN (Quantum=%d)              ║\n", quantum);
    printf("╚════════════════════════════════════════════════════╝\n");
    printf("Estrategia de bloqueos: PREVENCIÓN (desalojo equitativo)\n\n");
    
    ReadyQueue cola_copia;
    init_ready_queue(&cola_copia);
    while (!queue_vacia(q)) {
        encolar(&cola_copia, desencolar(q),0);
    }
    
    int tiempo_actual = 0;
    stats.cambios_contexto = 0;
    
    while (!queue_vacia(&cola_copia)) {
        PCB *proceso = desencolar(&cola_copia);
        
        proceso->tiempo_espera += tiempo_actual - proceso->ultimo_tiempo_en_cola;

        if (proceso->rafaga_restante == 0) {
            proceso->rafaga_restante = proceso->rafaga_total;
        }
        
        cambiar_estado(proceso, EJECUCION, tiempo_actual);
        stats.cambios_contexto++;
        
        int tiempo_ejecucion = (proceso->rafaga_restante < quantum) ?
                               proceso->rafaga_restante : quantum;
        
        printf("[t=%d] Ejecutando: %s (PID %d) | Tiempo: %d | Restante: %d\n",
               tiempo_actual, proceso->nombre, proceso->pid,
               tiempo_ejecucion, proceso->rafaga_restante - tiempo_ejecucion);
        
        tiempo_actual += tiempo_ejecucion;
        proceso->rafaga_restante -= tiempo_ejecucion;
        
        if (proceso->rafaga_restante == 0) {
            cambiar_estado(proceso, TERMINADO, tiempo_actual);
            printf("       → Terminado | Tiempo retorno: %d\n", proceso->tiempo_retorno);
            stats.tiempo_retorno_total += proceso->tiempo_retorno;
            stats.tiempo_espera_total += proceso->tiempo_espera;
        } else {
            cambiar_estado(proceso, LISTO, tiempo_actual);
            encolar(&cola_copia, proceso, tiempo_actual);
        }
    }
    
    stats.tiempo_total = tiempo_actual;
    stats.total_procesos = num_procesos;
}

/**
 * SJF (Shortest Job First) - No preventivo
 * Estrategia de bloqueos: PREVENCIÓN por ordenamiento óptimo
 */
int cmp_rafaga(const void *a, const void *b) {
    PCB *pa = *(PCB**)a;
    PCB *pb = *(PCB**)b;
    return (pa->rafaga_total - pb->rafaga_total);
}

void planificar_sjf(ReadyQueue *q, PCB *procesos[], int num_procesos) {
    printf("\n╔════════════════════════════════════════════════════╗\n");
    printf("║    ALGORITMO: SJF (Shortest Job First)             ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");
    printf("Estrategia de bloqueos: PREVENCIÓN por ordenamiento\n\n");
    
    PCB *lista[MAX_PROCESOS];
    int count = 0;
    while (!queue_vacia(q)) {
        lista[count++] = desencolar(q);
    }
    
    qsort(lista, count, sizeof(PCB*), cmp_rafaga);
    
    int tiempo_actual = 0;
    stats.cambios_contexto = 0;
    
    for (int i = 0; i < count; i++) {
        PCB *proceso = lista[i];
        cambiar_estado(proceso, EJECUCION, tiempo_actual);
        stats.cambios_contexto++;
        
        printf("[t=%d] Ejecutando: %s (PID %d) | Ráfaga: %d\n",
               tiempo_actual, proceso->nombre, proceso->pid, proceso->rafaga_total);
        
        tiempo_actual += proceso->rafaga_total;
        cambiar_estado(proceso, TERMINADO, tiempo_actual);
        
        printf("       → Terminado | Tiempo retorno: %d\n", proceso->tiempo_retorno);
        stats.tiempo_retorno_total += proceso->tiempo_retorno;
        stats.tiempo_espera_total += proceso->tiempo_espera;
    }
    
    stats.tiempo_total = tiempo_actual;
    stats.total_procesos = count;
}

/**
 * PRIORIDAD SIMPLE - No preventivo
 * Estrategia de bloqueos: PREVENCIÓN por orden de prioridad
 */
int cmp_prioridad(const void *a, const void *b) {
    PCB *pa = *(PCB**)a;
    PCB *pb = *(PCB**)b;
    if (pa->prioridad != pb->prioridad)
        return (pa->prioridad - pb->prioridad);
    return (pa->tiempo_llegada - pb->tiempo_llegada);
}

void planificar_prioridad(ReadyQueue *q, PCB *procesos[], int num_procesos) {
    printf("\n╔════════════════════════════════════════════════════╗\n");
    printf("║   ALGORITMO: PRIORIDAD SIMPLE (No preventivo)      ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");
    printf("Estrategia de bloqueos: PREVENCIÓN por prioridades\n\n");
    
    PCB *lista[MAX_PROCESOS];
    int count = 0;
    while (!queue_vacia(q)) {
        lista[count++] = desencolar(q);
    }
    
    qsort(lista, count, sizeof(PCB*), cmp_prioridad);
    
    int tiempo_actual = 0;
    stats.cambios_contexto = 0;
    
    for (int i = 0; i < count; i++) {
        PCB *proceso = lista[i];
        cambiar_estado(proceso, EJECUCION, tiempo_actual);
        stats.cambios_contexto++;
        
        printf("[t=%d] Ejecutando: %s (PID %d, Prioridad: %d) | Ráfaga: %d\n",
               tiempo_actual, proceso->nombre, proceso->pid,
               proceso->prioridad, proceso->rafaga_total);
        
        tiempo_actual += proceso->rafaga_total;
        cambiar_estado(proceso, TERMINADO, tiempo_actual);
        
        printf("       → Terminado | Tiempo retorno: %d\n", proceso->tiempo_retorno);
        stats.tiempo_retorno_total += proceso->tiempo_retorno;
        stats.tiempo_espera_total += proceso->tiempo_espera;
    }
    
    stats.tiempo_total = tiempo_actual;
    stats.total_procesos = count;
}

/**
 * CFS (Completely Fair Scheduler) con Árbol Rojo-Negro
 * Estrategia de bloqueos: PREVENCIÓN por equidad del vruntime
 */
void planificar_cfs(ReadyQueue *q, PCB *procesos[], int num_procesos) {
    printf("\n╔════════════════════════════════════════════════════╗\n");
    printf("║  ALGORITMO: CFS (Árbol Rojo-Negro, Vruntime)       ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");
    printf("Estrategia de bloqueos: PREVENCIÓN por equidad virtual\n\n");
    
    ArbolRB arbol;
    arbol.raiz = NULL;
    arbol.cantidad = 0;
    
    /* Insertar procesos en árbol */
    while (!queue_vacia(q)) {
        PCB *p = desencolar(q);
        p->peso_cfs = calcular_peso_cfs(p->nice);
        insertar_rb(&arbol, p);
    }
    
    int tiempo_actual = 0;
    int slice_minimo = 1;
    long suma_pesos = 0;
    
    for (int i = 0; i < num_procesos; i++) {
        suma_pesos += procesos[i]->peso_cfs;
    }
    
    stats.cambios_contexto = 0;
    
    while (arbol.cantidad > 0) {
        PCB *proceso = obtener_minimo(&arbol);
        if (proceso == NULL) break;
        
        cambiar_estado(proceso, EJECUCION, tiempo_actual);
        stats.cambios_contexto++;
        
        /* Calcular slice según peso */
        int slice = (proceso->peso_cfs * slice_minimo * num_procesos) / suma_pesos;
        if (slice < 1) slice = 1;
        if (slice > proceso->rafaga_restante) slice = proceso->rafaga_restante;
        
        printf("[t=%d] Ejecutando: %s (PID %d, nice=%d, vruntime=%ld) | Slice: %d\n",
               tiempo_actual, proceso->nombre, proceso->pid,
               proceso->nice, proceso->vruntime, slice);
        
        tiempo_actual += slice;
        proceso->rafaga_restante -= slice;
        
        /* Actualizar vruntime: delta = slice * (NICE_0_WEIGHT / peso) */
        long delta = (slice * NICE_0_WEIGHT) / proceso->peso_cfs;
        proceso->vruntime += delta;
        
        if (proceso->rafaga_restante <= 0) {
            cambiar_estado(proceso, TERMINADO, tiempo_actual);
            printf("       → Terminado | Vruntime final: %ld\n", proceso->vruntime);
            eliminar_rb(&arbol, proceso);
            stats.tiempo_retorno_total += proceso->tiempo_retorno;
            stats.tiempo_espera_total += proceso->tiempo_espera;
        } else {
            /* Reinsertar con nuevo vruntime */
            eliminar_rb(&arbol, proceso);
            insertar_rb(&arbol, proceso);
        }
    }
    
    stats.tiempo_total = tiempo_actual;
    stats.total_procesos = num_procesos;
}

/* ==================== IMPRIMIR ESTADÍSTICAS ==================== */

void imprimir_estadisticas(PCB *procesos[], int num_procesos) {
    printf("\n╔════════════════════════════════════════════════════╗\n");
    printf("║           ESTADÍSTICAS DEL SCHEDULING              ║\n");
    printf("╚════════════════════════════════════════════════════╝\n\n");
    
    float tiempo_espera_promedio = stats.tiempo_espera_total / (float)stats.total_procesos;
    float tiempo_retorno_promedio = stats.tiempo_retorno_total / (float)stats.total_procesos;
    float utilizacion = (stats.tiempo_total > 0) ?
                        ((-1)*(stats.tiempo_total - stats.tiempo_espera_total) * 100.0f / stats.tiempo_total) : 0;
    
    printf("Total de procesos:        %d\n", stats.total_procesos);
    printf("Tiempo total (makespan):  %d unidades\n", stats.tiempo_total);
    printf("Cambios de contexto:      %d\n", stats.cambios_contexto);
    printf("Tiempo espera promedio:   %.2f unidades\n", tiempo_espera_promedio);
    printf("Tiempo retorno promedio:  %.2f unidades\n", tiempo_retorno_promedio);
    printf("Utilización CPU:          %.2f%%\n", utilizacion);
    
    printf("\n┌─ Tabla de Procesos ─────────────────────────────────┐\n");
    printf("│ PID │ Nombre          │ Espera │ Retorno │ Respuesta │\n");
    printf("├─────┼─────────────────┼────────┼─────────┼───────────┤\n");
    
    for (int i = 0; i < num_procesos; i++) {
        printf("│ %3d │ %-15s │ %6d │ %7d │ %9d │\n",
               procesos[i]->pid, procesos[i]->nombre,
               procesos[i]->tiempo_espera, procesos[i]->tiempo_retorno,
               procesos[i]->tiempo_respuesta);
    }
    printf("└─────┴─────────────────┴────────┴─────────┴───────────┘\n");
}

/* ==================== FUNCIÓN PRINCIPAL ==================== */

int main() {
    printf("╔═══════════════════════════════════════════════════════════╗\n");
    printf("║   LABORATORIO #2: PLANIFICACIÓN Y CICLO DE VIDA           ║\n");
    printf("║     Universidad Santiago de Cali - Sistemas Operativos    ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n\n");
    
    /* Inicializar sistema */
    int total_recursos[MAX_RECURSOS] = {10, 5, 7};
    init_sistema_recursos(total_recursos);
    
    /* Crear procesos de prueba */
    PCB *procesos[MAX_PROCESOS];
    int max_res[MAX_RECURSOS] = {4, 2, 3};
    int nec_res[MAX_RECURSOS] = {2, 1, 1};
    
    procesos[0] = crear_proceso(1, "Editor", 8, 0, 3, 0, max_res, nec_res);
    procesos[1] = crear_proceso(2, "Compilador", 4, -5, 1, 0, max_res, nec_res);
    procesos[2] = crear_proceso(3, "Navegador", 12, 5, 4, 0, max_res, nec_res);
    procesos[3] = crear_proceso(4, "Servidor", 6, -2, 2, 0, max_res, nec_res);
    
    int num_procesos = 4;
    
    /* Menú de selección de algoritmos */
    printf("\nAlgoritmos a simular:\n");
    printf("  1) FCFS (First Come First Served)\n");
    printf("  2) Round Robin (Quantum=2)\n");
    printf("  3) SJF (Shortest Job First)\n");
    printf("  4) Prioridad Simple\n");
    printf("  5) CFS (Árbol Rojo-Negro)\n");
    printf("\n→ Ejecutando TODOS los algoritmos:\n");
    
    /* FCFS */
    ReadyQueue cola;
    init_ready_queue(&cola);
    memset(&stats, 0, sizeof(stats));
    for (int i = 0; i < num_procesos; i++) {
        procesos[i]->rafaga_restante = procesos[i]->rafaga_total;
        procesos[i]->estado = NUEVO;
        procesos[i]->tiempo_respuesta = -1;
        encolar(&cola, procesos[i],0);
    }
    planificar_fcfs(&cola, procesos, num_procesos);
    imprimir_estadisticas(procesos, num_procesos);
    
    /* Round Robin */
      init_ready_queue(&cola);
    memset(&stats, 0, sizeof(stats));
    for (int i = 0; i < num_procesos; i++) {
        procesos[i]->rafaga_restante = procesos[i]->rafaga_total;
        procesos[i]->estado = NUEVO;
        procesos[i]->tiempo_respuesta = -1;
        procesos[i]->contador_desalojos = 0;
        procesos[i]->tiempo_espera = 0;
        procesos[i]->ultimo_tiempo_en_cola = 0;
        encolar(&cola, procesos[i], 0);
    }
    planificar_round_robin(&cola, QUANTUM, procesos, num_procesos);
    imprimir_estadisticas(procesos, num_procesos);
    
    /* SJF */
    init_ready_queue(&cola);
    memset(&stats, 0, sizeof(stats));
    for (int i = 0; i < num_procesos; i++) {
        procesos[i]->rafaga_restante = procesos[i]->rafaga_total;
        procesos[i]->estado = NUEVO;
        procesos[i]->tiempo_respuesta = -1;
        encolar(&cola, procesos[i],0);
    }
    planificar_sjf(&cola, procesos, num_procesos);
    imprimir_estadisticas(procesos, num_procesos);
    
    /* Prioridad */
    init_ready_queue(&cola);
    memset(&stats, 0, sizeof(stats));
    for (int i = 0; i < num_procesos; i++) {
        procesos[i]->rafaga_restante = procesos[i]->rafaga_total;
        procesos[i]->estado = NUEVO;
        procesos[i]->tiempo_respuesta = -1;
        encolar(&cola, procesos[i],0);
    }
    planificar_prioridad(&cola, procesos, num_procesos);
    imprimir_estadisticas(procesos, num_procesos);
    
    /* CFS */
    init_ready_queue(&cola);
    memset(&stats, 0, sizeof(stats));
    for (int i = 0; i < num_procesos; i++) {
        procesos[i]->rafaga_restante = procesos[i]->rafaga_total;
        procesos[i]->estado = NUEVO;
        procesos[i]->tiempo_respuesta = -1;
        procesos[i]->vruntime = 0;
        encolar(&cola, procesos[i],0);
    }
    planificar_cfs(&cola, procesos, num_procesos);
    imprimir_estadisticas(procesos, num_procesos);
    
    /* Limpiar */
    for (int i = 0; i < num_procesos; i++) {
        liberar_pcb(procesos[i]);
    }
    
    
    return 0;
}