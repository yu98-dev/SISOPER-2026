/*
 * processes.c - Implementacion corregida
 * Universidad Santiago de Cali, 2026A
 * Laboratorio #2 Sistemas Operativos - Planificacion y Representacion de Procesos
 *
 * Algoritmos:
 *  1) FCFS
 *  2) Round Robin
 *  3) SJF / Shortest Job First
 *  4) Prioridad simple
 *  5) CFS simplificado con arbol rojo-negro Left-Leaning Red-Black
 *
 * Gestion de interbloqueos:
 *  - Evitacion: Algoritmo del banquero antes de asignar recursos.
 *  - Deteccion: revision de procesos que no pueden finalizar con recursos disponibles.
 *  - Recuperacion: aborta una victima bloqueada si se detecta deadlock.
 *
 * COMPILAR:
 *   gcc -Wall -std=c99 processes_corregido.c -o scheduler_lab
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>
#include "processes.h"

#define MAX_PROCESOS 10
#define MAX_RECURSOS 3
#define NUM_REGISTROS 8
#define QUANTUM 2
#define NICE_0_WEIGHT 1024
#define RED true
#define BLACK false

/*
 * Documentación inline:
 * - Los comentarios siguientes describen las estructuras de datos y
 *   las funciones principales utilizadas por los planificadores y
 *   el manejo de recursos (Banquero / deadlock).
 * - Está organizado por secciones: tipos base, utilidades, cola FIFO,
 *   ciclo de vida del PCB, algoritmo del banquero, árbol LLRB (CFS),
 *   comparadores y planificadores.
 */

/* ==================== TIPOS BASE ==================== */

/* Tipos definidos en processes.h */

/* Variables estaticas del modulo */
static EstadoSistema sistema_recursos;
static EstadisticasScheduling stats;

/* ==================== UTILIDADES ==================== */

const char *estado_a_texto(EstadoProceso e) {
    switch (e) {
        case NUEVO: return "NUEVO";
        case LISTO: return "LISTO";
        case EJECUCION: return "EJECUCION";
        case BLOQUEADO: return "BLOQUEADO";
        case TERMINADO: return "TERMINADO";
        default: return "DESCONOCIDO";
    }
}

/* estado_a_texto: utilidad simple para convertir el enum a cadena
 * usada al imprimir la tabla de procesos.
 */

void reset_stats(void) {
    memset(&stats, 0, sizeof(stats));
}

/* ==================== COLA FIFO ==================== */

void init_ready_queue(ReadyQueue *q) {
    q->frente = 0;
    q->final = 0;
    q->cantidad = 0;
    memset(q->procesos, 0, sizeof(q->procesos));
}

/* init_ready_queue: inicializa la cola circular poniendo indices a 0
 * y limpiando punteros. Llamar antes de cargar_cola.
 */

bool queue_vacia(const ReadyQueue *q) {
    return q->cantidad == 0;
}

bool queue_llena(const ReadyQueue *q) {
    return q->cantidad == MAX_PROCESOS;
}

void encolar(ReadyQueue *q, PCB *p, int tiempo_actual) {
    if (queue_llena(q)) {
        fprintf(stderr, "Error: cola de listos llena.\n");
        return;
    }

    q->procesos[q->final] = p;
    q->final = (q->final + 1) % MAX_PROCESOS;
    q->cantidad++;

    p->estado = LISTO;
    p->ultimo_tiempo_en_cola = tiempo_actual;
}

/* encolar: añade un PCB a la cola de listos y actualiza el
 * ultimo_tiempo_en_cola (usado para calcular espera en RR).
 */

PCB *desencolar(ReadyQueue *q) {
    if (queue_vacia(q)) return NULL;

    PCB *p = q->procesos[q->frente];
    q->procesos[q->frente] = NULL;
    q->frente = (q->frente + 1) % MAX_PROCESOS;
    q->cantidad--;
    return p;
}

/* ==================== PCB Y CICLO DE VIDA ==================== */

int calcular_peso_cfs(int nice) {
    static const int pesos[] = {
        88761, 71755, 56483, 46273, 36291,
        29154, 23254, 18705, 14949, 11916,
        9548, 7620, 6100, 4904, 3906,
        3121, 2501, 1991, 1586, 1277,
        1024, 820, 655, 526, 423,
        335, 272, 215, 172, 137,
        110, 87, 70, 56, 45,
        36, 29, 23, 18, 15
    };

    if (nice < -20) nice = -20;
    if (nice > 19) nice = 19;
    return pesos[nice + 20];
}

/*
 * calcular_peso_cfs
 * ------------------
 * Descripción:
 *   Traduce un valor nice (rango -20..19) a un peso entero usado por
 *   el scheduler CFS. Los pesos están basados en la tabla de Linux
 *   (aproximada) y determinan la proporción de tiempo de CPU que
 *   recibirá un proceso.
 * Entradas:
 *   - nice: prioridad de usuario (valor entero; menor => más favorable)
 * Salida:
 *   - entero > 0 que representa el peso relativo del proceso
 * Efectos secundarios:
 *   - ninguno (función pura)
 */

PCB *crear_proceso(int pid,
                   const char *nombre,
                   int rafaga,
                   int nice_val,
                   int prioridad,
                   int tiempo_llegada,
                   const int recursos_maximos[MAX_RECURSOS]) {
    PCB *nuevo = (PCB *)malloc(sizeof(PCB));
    if (!nuevo) {
        fprintf(stderr, "Error: no se pudo reservar memoria para PCB.\n");
        exit(EXIT_FAILURE);
    }

    memset(nuevo, 0, sizeof(PCB));
    nuevo->pid = pid;
    strncpy(nuevo->nombre, nombre, sizeof(nuevo->nombre) - 1);
    nuevo->prioridad = prioridad;
    nuevo->rafaga_total = rafaga;
    nuevo->rafaga_restante = rafaga;
    nuevo->tiempo_llegada = tiempo_llegada;
    nuevo->tiempo_respuesta = -1;
    nuevo->estado = NUEVO;
    nuevo->nice = nice_val;
    nuevo->peso_cfs = calcular_peso_cfs(nice_val);
    nuevo->vruntime = 0;
    nuevo->color = RED;

    for (int i = 0; i < MAX_RECURSOS; i++) {
        nuevo->recursos_maximos[i] = recursos_maximos[i];
        nuevo->recursos_necesarios[i] = recursos_maximos[i];
        nuevo->recursos_asignados[i] = 0;
    }

    for (int i = 0; i < NUM_REGISTROS; i++) {
        nuevo->imagen.registros[i] = pid * 100 + i;
    }
    nuevo->imagen.program_counter = 0;
    nuevo->imagen.stack_pointer = 1024;

    return nuevo;
}

/*
 * crear_proceso
 * -------------
 * Descripción:
 *   Reserva memoria e inicializa un PCB con valores por defecto y
 *   con la "imagen" simulada del proceso (registros, pc, sp).
 * Entradas:
 *   - pid: identificador numérico del proceso
 *   - nombre: cadena con nombre descriptivo
 *   - rafaga: tiempo total de CPU requerido
 *   - nice_val: valor nice para CFS
 *   - prioridad: prioridad del planificador por prioridad
 *   - tiempo_llegada: instante en que el proceso entra al sistema
 *   - recursos_maximos: vector con demanda máxima esperada (banquero)
 * Salida:
 *   - puntero a PCB inicializado (malloc)
 * Efectos secundarios:
 *   - reserva memoria; el llamador debe liberar con liberar_pcb
 */

/* crear_proceso: reserva e inicializa un PCB con valores por defecto.
 * - Inicializa la "imagen" con datos simulados para facilitar trazas.
 * - recursos_maximos define la demanda máxima esperada para el banquero.
 */

void liberar_pcb(PCB *p) {
    free(p);
}

/*
 * liberar_pcb
 * -----------
 * Descripción:
 *   Libera la memoria asociada a un PCB creado por crear_proceso.
 * Entradas:
 *   - p: puntero a PCB (puede ser NULL)
 * Salida:
 *   - ninguna
 * Efectos secundarios:
 *   - libera memoria heap
 */

void ciclo_vida_proceso(PCB *p, EstadoProceso nuevo_estado, int tiempo_actual) {
    if (!p || p->estado == TERMINADO) return;

    EstadoProceso anterior = p->estado;
    bool transicion_valida = false;

    switch (anterior) {
        case NUEVO:
            transicion_valida = (nuevo_estado == LISTO);
            break;
        case LISTO:
            transicion_valida = (nuevo_estado == EJECUCION || nuevo_estado == BLOQUEADO);
            break;
        case EJECUCION:
            transicion_valida = (nuevo_estado == LISTO || nuevo_estado == BLOQUEADO || nuevo_estado == TERMINADO);
            break;
        case BLOQUEADO:
            transicion_valida = (nuevo_estado == LISTO || nuevo_estado == TERMINADO);
            break;
        case TERMINADO:
            transicion_valida = false;
            break;
    }

    if (!transicion_valida) return;

    p->estado = nuevo_estado;

    if (nuevo_estado == EJECUCION && p->tiempo_respuesta == -1) {
        p->tiempo_respuesta = tiempo_actual - p->tiempo_llegada;
    }

    if (anterior == EJECUCION && nuevo_estado == LISTO) {
        p->contador_desalojos++;
    }

    if (nuevo_estado == TERMINADO) {
        p->tiempo_retorno = tiempo_actual - p->tiempo_llegada;
        if (p->tiempo_espera == 0) {
            p->tiempo_espera = p->tiempo_retorno - p->rafaga_total;
        }
    }
}

/*
 * ciclo_vida_proceso
 * ------------------
 * Descripción:
 *   Valida y aplica transiciones de estado para un proceso.
 *   Calcula tiempos de respuesta, retorno, espera y cuenta desalojos.
 * Entradas:
 *   - p: PCB a actualizar
 *   - nuevo_estado: estado al que se quiere llevar el proceso
 *   - tiempo_actual: tiempo lógico de la simulación
 * Salidas:
 *   - actualiza campos internos del PCB
 * Efectos secundarios:
 *   - modifica p si la transición es válida; no hace nada si el
 *     proceso ya está en TERMINADO
 */

/* ciclo_vida_proceso: valida y aplica transiciones de estado del PCB.
 * - Actualiza métricas: tiempo de respuesta (primera ejecución),
 *   conteo de desalojos, tiempo de retorno y tiempo de espera.
 * - Evita transiciones inválidas (por ejemplo, de TERMINADO a otro estado).
 */

void resetear_procesos(PCB *procesos[], int n) {
    for (int i = 0; i < n; i++) {
        PCB *p = procesos[i];
        p->rafaga_restante = p->rafaga_total;
        p->tiempo_respuesta = -1;
        p->tiempo_retorno = 0;
        p->tiempo_espera = 0;
        p->ultimo_tiempo_en_cola = p->tiempo_llegada;
        p->contador_desalojos = 0;
        p->estado = NUEVO;
        p->vruntime = 0;
        p->izq = NULL;
        p->der = NULL;
        p->color = RED;
        p->recursos_otorgados = false;

        for (int j = 0; j < MAX_RECURSOS; j++) {
            p->recursos_asignados[j] = 0;
            p->recursos_necesarios[j] = p->recursos_maximos[j];
        }
    }
}

/*
 * resetear_procesos
 * -----------------
 * Descripción:
 *   Reestablece los campos temporales de un conjunto de PCBs para
 *   re-ejecutar los experimentos de planificación desde cero.
 * Entradas:
 *   - procesos: arreglo de punteros a PCB
 *   - n: cantidad de procesos en el arreglo
 * Efectos secundarios:
 *   - modifica cada PCB (rafaga_restante, tiempos, flags de recursos,
 *     punteros de árbol, etc.)
 */

/* ==================== BANQUERO / DEADLOCK ==================== */

void init_sistema_recursos(const int total_recursos[MAX_RECURSOS]) {
    for (int i = 0; i < MAX_RECURSOS; i++) {
        sistema_recursos.total[i] = total_recursos[i];
        sistema_recursos.disponible[i] = total_recursos[i];
    }
}

/*
 * init_sistema_recursos
 * ---------------------
 * Descripción:
 *   Inicializa el estado global de recursos (sistema_recursos) con
 *   los totales y marca todo como disponible.
 * Entradas:
 *   - total_recursos: vector con cantidad total por tipo
 * Efectos secundarios:
 *   - modifica la variable global sistema_recursos
 */

/* init_sistema_recursos: configura la cantidad total y disponible de
 * recursos al inicio de cada experimento de scheduling.
 */

bool proceso_puede_finalizar(const PCB *p, const int trabajo[MAX_RECURSOS]) {
    if (p->estado == TERMINADO) return true;

    for (int i = 0; i < MAX_RECURSOS; i++) {
        if (p->recursos_necesarios[i] > trabajo[i]) return false;
    }
    return true;
}

/*
 * proceso_puede_finalizar
 * -----------------------
 * Descripción:
 *   Determina si un proceso puede finalizar con la cantidad de
 *   recursos representada en trabajo (vector de recursos
 *   hipotéticamente disponibles).
 * Entradas:
 *   - p: proceso a evaluar
 *   - trabajo: vector de recursos disponibles hipotéticos
 * Salida:
 *   - true si p puede terminar con esos recursos, false en caso contrario
 * Efectos secundarios:
 *   - ninguno
 */

bool es_estado_seguro(PCB *procesos[], int num_procesos) {
    int trabajo[MAX_RECURSOS];
    bool terminado[MAX_PROCESOS] = {false};

    for (int i = 0; i < MAX_RECURSOS; i++) {
        trabajo[i] = sistema_recursos.disponible[i];
    }

    for (int i = 0; i < num_procesos; i++) {
        terminado[i] = (procesos[i]->estado == TERMINADO);
    }

    bool progreso;
    do {
        progreso = false;

        for (int i = 0; i < num_procesos; i++) {
            if (!terminado[i] && proceso_puede_finalizar(procesos[i], trabajo)) {
                for (int r = 0; r < MAX_RECURSOS; r++) {
                    trabajo[r] += procesos[i]->recursos_asignados[r];
                }
                terminado[i] = true;
                progreso = true;
            }
        }
    } while (progreso);

    for (int i = 0; i < num_procesos; i++) {
        if (!terminado[i]) return false;
    }
    return true;
}

/*
 * es_estado_seguro
 * ----------------
 * Descripción:
 *   Implementa la verificación de estado seguro del algoritmo del
 *   banquero: simula la finalización de procesos para determinar si se
 *   puede llegar a un estado donde todos finalizan.
 * Entradas:
 *   - procesos: arreglo de PCBs
 *   - num_procesos: número de procesos
 * Salida:
 *   - true si el estado es seguro, false si existe riesgo de deadlock
 * Efectos secundarios:
 *   - ninguno (usa copias locales de vectores)
 */

bool asignar_recursos_banquero(PCB *p, const int solicitud[MAX_RECURSOS], PCB *procesos[], int num_procesos) {
    if (p->recursos_otorgados) return true;

    for (int i = 0; i < MAX_RECURSOS; i++) {
        if (solicitud[i] > p->recursos_necesarios[i]) return false;
        if (solicitud[i] > sistema_recursos.disponible[i]) return false;
    }

    for (int i = 0; i < MAX_RECURSOS; i++) {
        sistema_recursos.disponible[i] -= solicitud[i];
        p->recursos_asignados[i] += solicitud[i];
        p->recursos_necesarios[i] -= solicitud[i];
    }

    if (!es_estado_seguro(procesos, num_procesos)) {
        for (int i = 0; i < MAX_RECURSOS; i++) {
            sistema_recursos.disponible[i] += solicitud[i];
            p->recursos_asignados[i] -= solicitud[i];
            p->recursos_necesarios[i] += solicitud[i];
        }
        return false;
    }

    p->recursos_otorgados = true;
    return true;
}

/*
 * asignar_recursos_banquero
 * -------------------------
 * Descripción:
 *   Intenta conceder a p la solicitud completa de recursos usando
 *   el algoritmo del banquero. Realiza una asignación tentativa y
 *   verifica que el sistema quede en estado seguro. Si no es seguro,
 *   revierte la asignación.
 * Entradas:
 *   - p: PCB solicitante
 *   - solicitud: vector con cantidades solicitadas por tipo
 *   - procesos, num_procesos: conjunto completo para la verificación
 * Salida:
 *   - true si la asignación fue otorgada, false si se rechaza
 * Efectos secundarios:
 *   - en caso exitoso: actualiza sistema_recursos y campos del PCB
 */

/* asignar_recursos_banquero: intenta conceder una solicitud completa
 * de recursos a p usando el algoritmo del banquero. Realiza una
 * asignación tentativa y verifica el estado seguro; en caso de fallo
 * revierte los cambios.
 */

void liberar_recursos_proceso(PCB *p) {
    for (int i = 0; i < MAX_RECURSOS; i++) {
        sistema_recursos.disponible[i] += p->recursos_asignados[i];
        p->recursos_necesarios[i] += p->recursos_asignados[i];
        p->recursos_asignados[i] = 0;
    }
    p->recursos_otorgados = false;
}

/*
 * liberar_recursos_proceso
 * ------------------------
 * Descripción:
 *   Devuelve al pool del sistema los recursos asignados a p y
 *   restablece sus necesidades.
 * Entradas:
 *   - p: PCB cuya asignación se libera
 * Efectos secundarios:
 *   - modifica sistema_recursos y campos de p
 */

bool detectar_deadlock(PCB *procesos[], int num_procesos) {
    int trabajo[MAX_RECURSOS];
    bool terminado[MAX_PROCESOS] = {false};

    for (int i = 0; i < MAX_RECURSOS; i++) {
        trabajo[i] = sistema_recursos.disponible[i];
    }

    bool progreso;
    do {
        progreso = false;
        for (int i = 0; i < num_procesos; i++) {
            if (!terminado[i] && procesos[i]->estado != TERMINADO && proceso_puede_finalizar(procesos[i], trabajo)) {
                for (int r = 0; r < MAX_RECURSOS; r++) {
                    trabajo[r] += procesos[i]->recursos_asignados[r];
                }
                terminado[i] = true;
                progreso = true;
            }
        }
    } while (progreso);

    for (int i = 0; i < num_procesos; i++) {
        if (procesos[i]->estado != TERMINADO && !terminado[i]) return true;
    }
    return false;
}

/*
 * detectar_deadlock
 * ------------------
 * Descripción:
 *   Intenta detectar la existencia de un deadlock mediante una
 *   simulación similar a la comprobación del banquero: si al final
 *   existe algún proceso no terminado que no pueda finalizar, se
 *   considera deadlock.
 * Entradas:
 *   - procesos: arreglo de PCBs
 *   - num_procesos: cantidad en el arreglo
 * Salida:
 *   - true si se detecta potencial deadlock, false si no
 */

void recuperar_deadlock(PCB *procesos[], int num_procesos) {
    int victima = -1;
    int peor_prioridad = INT_MIN;

    for (int i = 0; i < num_procesos; i++) {
        if (procesos[i]->estado == BLOQUEADO && procesos[i]->prioridad > peor_prioridad) {
            peor_prioridad = procesos[i]->prioridad;
            victima = i;
        }
    }

    if (victima >= 0) {
        printf("  [deadlock] Victima: %s (PID %d). Se aborta y se liberan recursos.\n",
               procesos[victima]->nombre, procesos[victima]->pid);
        liberar_recursos_proceso(procesos[victima]);
        procesos[victima]->estado = TERMINADO;
    }
}

/*
 * recuperar_deadlock
 * ------------------
 * Descripción:
 *   Estrategia simple de recuperación: selecciona una "víctima"
 *   bloqueada (basada en peor prioridad) y la aborta liberando sus
 *   recursos para romper el deadlock.
 * Entradas:
 *   - procesos: arreglo de PCBs
 *   - num_procesos: longitud del arreglo
 * Efectos secundarios:
 *   - puede marcar un proceso como TERMINADO y liberar recursos
 */

bool preparar_recursos_o_bloquear(PCB *p, PCB *procesos[], int num_procesos, int tiempo_actual) {
    int solicitud[MAX_RECURSOS];

    for (int i = 0; i < MAX_RECURSOS; i++) {
        solicitud[i] = p->recursos_necesarios[i];
    }

    if (asignar_recursos_banquero(p, solicitud, procesos, num_procesos)) {
        return true;
    }

    printf("[t=%d] %s BLOQUEADO: solicitud no segura por banquero.\n", tiempo_actual, p->nombre);
    ciclo_vida_proceso(p, BLOQUEADO, tiempo_actual);

    if (detectar_deadlock(procesos, num_procesos)) {
        recuperar_deadlock(procesos, num_procesos);
    }

    return false;
}

/*
 * preparar_recursos_o_bloquear
 * ----------------------------
 * Descripción:
 *   Intenta asignar todos los recursos que le resten al proceso p
 *   (solicitud completa). Si la asignación es insegura, marca p
 *   como BLOQUEADO y ejecuta detección/recuperación de deadlock.
 * Entradas:
 *   - p: proceso solicitante
 *   - procesos, num_procesos: contexto para el banquero
 *   - tiempo_actual: tiempo para mensajes y marcar bloqueo
 * Salida:
 *   - true si los recursos fueron concedidos, false si se bloqueó
 */

/* ==================== ARBOL ROJO-NEGRO LLRB PARA CFS ==================== */

static bool rb_es_rojo(PCB *n) {
    return n != NULL && n->color == RED;
}

/* rb_es_rojo: helper para el LLRB: indica si un nodo es rojo. */

static int rb_comparar(const PCB *a, const PCB *b) {
    if (a->vruntime < b->vruntime) return -1;
    if (a->vruntime > b->vruntime) return 1;
    return a->pid - b->pid;
}

/* rb_comparar: compara por vruntime y en empate por pid.
 * Usado para mantener orden en el árbol CFS (menor vruntime = mayor prioridad).
 */

static PCB *rb_rotar_izquierda(PCB *h) {
    PCB *x = h->der;
    h->der = x->izq;
    x->izq = h;
    x->color = h->color;
    h->color = RED;
    return x;
}

/* rb_rotar_izquierda / rb_rotar_derecha / rb_flip_colores:
 * Rotaciones y ajustes de color para mantener las propiedades LLRB.
 */

static PCB *rb_rotar_derecha(PCB *h) {
    PCB *x = h->izq;
    h->izq = x->der;
    x->der = h;
    x->color = h->color;
    h->color = RED;
    return x;
}

static void rb_flip_colores(PCB *h) {
    h->color = !h->color;
    if (h->izq) h->izq->color = !h->izq->color;
    if (h->der) h->der->color = !h->der->color;
}

static PCB *rb_balancear(PCB *h) {
    if (rb_es_rojo(h->der) && !rb_es_rojo(h->izq)) h = rb_rotar_izquierda(h);
    if (rb_es_rojo(h->izq) && rb_es_rojo(h->izq->izq)) h = rb_rotar_derecha(h);
    if (rb_es_rojo(h->izq) && rb_es_rojo(h->der)) rb_flip_colores(h);
    return h;
}

/* rb_balancear: aplica las correcciones estándar tras inserciones/eliminaciones. */

static PCB *rb_mover_rojo_izquierda(PCB *h) {
    rb_flip_colores(h);
    if (h->der && rb_es_rojo(h->der->izq)) {
        h->der = rb_rotar_derecha(h->der);
        h = rb_rotar_izquierda(h);
        rb_flip_colores(h);
    }
    return h;
}

/* rb_mover_rojo_izquierda: ajuste auxiliar usado en eliminación mínima. */

static PCB *rb_insertar_nodo(PCB *h, PCB *nodo) {
    if (h == NULL) {
        nodo->izq = NULL;
        nodo->der = NULL;
        nodo->color = RED;
        return nodo;
    }

    if (rb_comparar(nodo, h) < 0) {
        h->izq = rb_insertar_nodo(h->izq, nodo);
    } else {
        h->der = rb_insertar_nodo(h->der, nodo);
    }

    return rb_balancear(h);
}

/* rb_insertar_nodo: inserta recursivamente por orden y devuelve nuevo subárbol balanceado. */

/* rb_insertar_nodo / rb_insertar: inserción LLRB por vruntime.
 * El árbol mantiene el orden para seleccionar el proceso con menor
 * vruntime en CFS. Implementación simplificada (Left-Leaning RB).
 */

void rb_insertar(ArbolRB *arbol, PCB *nodo) {
    nodo->izq = NULL;
    nodo->der = NULL;
    nodo->color = RED;
    arbol->raiz = rb_insertar_nodo(arbol->raiz, nodo);
    if (arbol->raiz) arbol->raiz->color = BLACK;
    arbol->cantidad++;
}

/* rb_insertar: envoltorio que inserta un nodo en el ArbolRB y asegura la raíz negra. */

PCB *rb_minimo(PCB *h) {
    if (!h) return NULL;
    while (h->izq) h = h->izq;
    return h;
}

/* rb_minimo: retorna el nodo con menor vruntime (extremo izquierdo). */

static PCB *rb_eliminar_min_nodo(PCB *h) {
    if (h->izq == NULL) {
        return NULL;
    }

    if (!rb_es_rojo(h->izq) && !rb_es_rojo(h->izq->izq)) {
        h = rb_mover_rojo_izquierda(h);
    }

    h->izq = rb_eliminar_min_nodo(h->izq);
    return rb_balancear(h);
}

/* rb_eliminar_min_nodo: elimina recursivamente el mínimo del subárbol. */

PCB *rb_extraer_minimo(ArbolRB *arbol) {
    PCB *min = rb_minimo(arbol->raiz);
    if (!min) return NULL;

    if (!rb_es_rojo(arbol->raiz->izq) && !rb_es_rojo(arbol->raiz->der)) {
        arbol->raiz->color = RED;
    }

    arbol->raiz = rb_eliminar_min_nodo(arbol->raiz);
    if (arbol->raiz) arbol->raiz->color = BLACK;
    arbol->cantidad--;

    min->izq = NULL;
    min->der = NULL;
    min->color = RED;
    return min;
}

/* rb_extraer_minimo: extrae y devuelve el PCB con menor vruntime del árbol.
 * Se usa en CFS para seleccionar el siguiente proceso a ejecutar.
 */

void rb_imprimir_inorden(PCB *n, int nivel) {
    if (!n) return;
    rb_imprimir_inorden(n->izq, nivel + 1);
    printf("  nivel=%d PID=%d vruntime=%ld color=%s\n",
           nivel, n->pid, n->vruntime, n->color == RED ? "ROJO" : "NEGRO");
    rb_imprimir_inorden(n->der, nivel + 1);
}

/* rb_imprimir_inorden: utilidad de depuración que muestra el árbol ordenado. */

/* ==================== COMPARADORES ==================== */

int cmp_rafaga(const void *a, const void *b) {
    const PCB *pa = *(const PCB * const *)a;
    const PCB *pb = *(const PCB * const *)b;

    if (pa->rafaga_total != pb->rafaga_total) {
        return pa->rafaga_total - pb->rafaga_total;
    }
    return pa->tiempo_llegada - pb->tiempo_llegada;
}

/* cmp_rafaga / cmp_prioridad: comparadores para qsort utilizados por SJF y Prioridad. */

/* Comparadores usados por qsort:
 * - cmp_rafaga: orden por rafaga_total para SJF
 * - cmp_prioridad: orden por prioridad para planificador por prioridad
 */

int cmp_prioridad(const void *a, const void *b) {
    const PCB *pa = *(const PCB * const *)a;
    const PCB *pb = *(const PCB * const *)b;

    if (pa->prioridad != pb->prioridad) {
        return pa->prioridad - pb->prioridad;
    }
    return pa->tiempo_llegada - pb->tiempo_llegada;
}

/* ==================== PLANIFICADORES ==================== */

void terminar_proceso(PCB *p, int tiempo_actual) {
    ciclo_vida_proceso(p, TERMINADO, tiempo_actual);
    liberar_recursos_proceso(p);
    stats.tiempo_retorno_total += p->tiempo_retorno;
    stats.tiempo_espera_total += p->tiempo_espera;
}

/*
 * terminar_proceso
 * -----------------
 * Descripción:
 *   Marca un proceso como TERMINADO, libera sus recursos y acumula
 *   métricas globales para impresión posterior.
 * Entradas:
 *   - p: PCB que termina
 *   - tiempo_actual: tiempo lógico de finalización
 */

void planificar_fcfs(ReadyQueue *q, PCB *procesos[], int num_procesos) {
    printf("\n╔════════════════════════════════════════════════════╗\n");
    printf("║        ALGORITMO: FCFS (First Come First Served)   ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");
    printf("Estrategia de bloqueos: PREVENCIÓN (orden FIFO evita inanición)\n\n");
    int tiempo_actual = 0;

    while (!queue_vacia(q)) {
        PCB *p = desencolar(q);
        if (tiempo_actual < p->tiempo_llegada) tiempo_actual = p->tiempo_llegada;

        if (!preparar_recursos_o_bloquear(p, procesos, num_procesos, tiempo_actual)) {
            tiempo_actual++;
            if (p->estado != TERMINADO) encolar(q, p, tiempo_actual);
            continue;
        }

        p->tiempo_espera = tiempo_actual - p->tiempo_llegada;
        ciclo_vida_proceso(p, EJECUCION, tiempo_actual);
        stats.cambios_contexto++;

        printf("[t=%d] Ejecuta %-12s PID=%d rafaga=%d\n", tiempo_actual, p->nombre, p->pid, p->rafaga_total);
        tiempo_actual += p->rafaga_total;
        p->rafaga_restante = 0;
        terminar_proceso(p, tiempo_actual);
    }

    stats.total_procesos = num_procesos;
    stats.tiempo_total = tiempo_actual;
}

/* planificar_fcfs: ejecuta cada proceso completamente por orden de llegada.
 * - Intenta preparar recursos antes de ejecutar; si falla se bloquea.
 * - Actualiza métricas y muestra trazas de ejecución.
 */

/* planificar_fcfs: ejecuta cada proceso en orden de llegada (FIFO).
 * - Intenta obtener recursos antes de ejecutar; si se bloquea, se
 *   gestiona deadlock y se reintenta.
 */

void planificar_round_robin(ReadyQueue *q, int quantum, PCB *procesos[], int num_procesos) {
    printf("\n╔════════════════════════════════════════════════════╗\n");
    printf("║    ALGORITMO: ROUND ROBIN (Quantum=%d)              ║\n", quantum);
    printf("╚════════════════════════════════════════════════════╝\n");
    printf("Estrategia de bloqueos: PREVENCIÓN (desalojo equitativo)\n\n");
    int tiempo_actual = 0;

    while (!queue_vacia(q)) {
        PCB *p = desencolar(q);
        if (tiempo_actual < p->tiempo_llegada) tiempo_actual = p->tiempo_llegada;

        p->tiempo_espera += tiempo_actual - p->ultimo_tiempo_en_cola;

        if (!preparar_recursos_o_bloquear(p, procesos, num_procesos, tiempo_actual)) {
            tiempo_actual++;
            if (p->estado != TERMINADO) encolar(q, p, tiempo_actual);
            continue;
        }

        ciclo_vida_proceso(p, EJECUCION, tiempo_actual);
        stats.cambios_contexto++;

        int ejecucion = (p->rafaga_restante < quantum) ? p->rafaga_restante : quantum;
        printf("[t=%d] Ejecuta %-12s PID=%d slice=%d restante_final=%d\n",
               tiempo_actual, p->nombre, p->pid, ejecucion, p->rafaga_restante - ejecucion);

        tiempo_actual += ejecucion;
        p->rafaga_restante -= ejecucion;

        if (p->rafaga_restante == 0) {
            terminar_proceso(p, tiempo_actual);
        } else {
            ciclo_vida_proceso(p, LISTO, tiempo_actual);
            encolar(q, p, tiempo_actual);
        }
    }

    stats.total_procesos = num_procesos;
    stats.tiempo_total = tiempo_actual;
}

/* planificar_round_robin: ejecuta por slices de quantum y reencola
 * procesos que no terminan. Calcula espera incrementalmente.
 */

/* planificar_round_robin: ejecuta por slices de quantum y reencola
 * procesos no terminados. Actualiza tiempos de espera incrementales.
 */

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

    qsort(lista, count, sizeof(PCB *), cmp_rafaga);

    int tiempo_actual = 0;
    for (int i = 0; i < count; i++) {
        PCB *p = lista[i];
        if (tiempo_actual < p->tiempo_llegada) tiempo_actual = p->tiempo_llegada;

        if (!preparar_recursos_o_bloquear(p, procesos, num_procesos, tiempo_actual)) {
            tiempo_actual++;
            i--;
            continue;
        }

        p->tiempo_espera = tiempo_actual - p->tiempo_llegada;
        ciclo_vida_proceso(p, EJECUCION, tiempo_actual);
        stats.cambios_contexto++;

        printf("[t=%d] Ejecuta %-12s PID=%d rafaga=%d\n", tiempo_actual, p->nombre, p->pid, p->rafaga_total);
        tiempo_actual += p->rafaga_total;
        p->rafaga_restante = 0;
        terminar_proceso(p, tiempo_actual);
    }

    stats.total_procesos = count;
    stats.tiempo_total = tiempo_actual;
}

/* planificar_sjf: ordena los procesos por rafaga_total y los ejecuta en ese orden.
 * - No es preemptive en esta implementación (ejecución completa por proceso).
 */

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

    qsort(lista, count, sizeof(PCB *), cmp_prioridad);

    int tiempo_actual = 0;
    for (int i = 0; i < count; i++) {
        PCB *p = lista[i];
        if (tiempo_actual < p->tiempo_llegada) tiempo_actual = p->tiempo_llegada;

        if (!preparar_recursos_o_bloquear(p, procesos, num_procesos, tiempo_actual)) {
            tiempo_actual++;
            i--;
            continue;
        }

        p->tiempo_espera = tiempo_actual - p->tiempo_llegada;
        ciclo_vida_proceso(p, EJECUCION, tiempo_actual);
        stats.cambios_contexto++;

        printf("[t=%d] Ejecuta %-12s PID=%d prioridad=%d rafaga=%d\n",
               tiempo_actual, p->nombre, p->pid, p->prioridad, p->rafaga_total);
        tiempo_actual += p->rafaga_total;
        p->rafaga_restante = 0;
        terminar_proceso(p, tiempo_actual);
    }

    stats.total_procesos = count;
    stats.tiempo_total = tiempo_actual;
}

/* planificar_prioridad: ejecuta procesos por orden de prioridad (menor valor primero).
 * - Implementación no preventiva: ejecuta cada proceso hasta terminar.
 */

void planificar_cfs(ReadyQueue *q, PCB *procesos[], int num_procesos) {
    printf("\n╔════════════════════════════════════════════════════╗\n");
    printf("║  ALGORITMO: CFS (Árbol Rojo-Negro)       ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");
    printf("Estrategia de bloqueos: PREVENCIÓN por equidad virtual\n\n");
    ArbolRB arbol = {NULL, 0};
    long suma_pesos = 0;

    while (!queue_vacia(q)) {
        PCB *p = desencolar(q);
        p->peso_cfs = calcular_peso_cfs(p->nice);
        suma_pesos += p->peso_cfs;
        rb_insertar(&arbol, p);
    }

    printf("Arbol inicial por vruntime:\n");
    rb_imprimir_inorden(arbol.raiz, 0);

    int tiempo_actual = 0;
    const int periodo_objetivo = 8;

    while (arbol.cantidad > 0) {
        PCB *p = rb_extraer_minimo(&arbol);
        if (!p) break;

        if (!preparar_recursos_o_bloquear(p, procesos, num_procesos, tiempo_actual)) {
            tiempo_actual++;
            if (p->estado != TERMINADO) {
                ciclo_vida_proceso(p, LISTO, tiempo_actual);
                rb_insertar(&arbol, p);
            }
            continue;
        }

        ciclo_vida_proceso(p, EJECUCION, tiempo_actual);
        stats.cambios_contexto++;

        int slice = (int)((long long)periodo_objetivo * p->peso_cfs / suma_pesos);
        if (slice < 1) slice = 1;
        if (slice > p->rafaga_restante) slice = p->rafaga_restante;

        printf("[t=%d] Ejecuta %-12s PID=%d nice=%d peso=%d vruntime=%ld slice=%d\n",
               tiempo_actual, p->nombre, p->pid, p->nice, p->peso_cfs, p->vruntime, slice);

        tiempo_actual += slice;
        p->rafaga_restante -= slice;
        p->vruntime += ((long)slice * NICE_0_WEIGHT) / p->peso_cfs;

        if (p->rafaga_restante == 0) {
            terminar_proceso(p, tiempo_actual);
            suma_pesos -= p->peso_cfs;
            if (suma_pesos <= 0) suma_pesos = 1;
        } else {
            ciclo_vida_proceso(p, LISTO, tiempo_actual);
            rb_insertar(&arbol, p);
        }
    }

    stats.total_procesos = num_procesos;
    stats.tiempo_total = tiempo_actual;
}

/* planificar_cfs: scheduler proporcional usando un árbol LLRB ordenado por vruntime.
 * - Inserta procesos, calcula slices proporcionales a peso_cfs y actualiza vruntime.
 */

/* planificar_cfs: scheduler proporcional simplificado.
 * - Inserta procesos en un árbol LLRB ordenado por vruntime.
 * - Calcula un slice proporcional al peso_cfs de cada proceso.
 */

/* Funcion requerida: llama el metodo de planificacion segun su id. */
void llamar_planificador_por_id(int id, ReadyQueue *cola, PCB *procesos[], int num_procesos) {
    switch (id) {
        case 1:
            planificar_fcfs(cola, procesos, num_procesos);
            break;
        case 2:
            planificar_round_robin(cola, QUANTUM, procesos, num_procesos);
            break;
        case 3:
            planificar_sjf(cola, procesos, num_procesos);
            break;
        case 4:
            planificar_prioridad(cola, procesos, num_procesos);
            break;
        case 5:
            planificar_cfs(cola, procesos, num_procesos);
            break;
        default:
            printf("Planificador no valido: %d\n", id);
            break;
    }
}

/* llamar_planificador_por_id: wrapper que selecciona el planificador según id.
 * - id 1..5 corresponden a FCFS, RR, SJF, Prioridad y CFS respectivamente.
 */

/* ==================== IMPRESION ==================== */

void cargar_cola(ReadyQueue *cola, PCB *procesos[], int n) {
    init_ready_queue(cola);
    for (int i = 0; i < n; i++) {
        encolar(cola, procesos[i], procesos[i]->tiempo_llegada);
    }
}

/* cargar_cola: inicializa la ReadyQueue y encola los procesos según su
 * tiempo_llegada.
 */

void imprimir_estadisticas(PCB *procesos[], int num_procesos) {
    printf("\n╔════════════════════════════════════════════════════╗\n");
    printf("║           ESTADÍSTICAS DEL SCHEDULING              ║\n");
    printf("╚════════════════════════════════════════════════════╝\n\n");

    float espera_prom = (stats.total_procesos > 0)
        ? stats.tiempo_espera_total / (float)stats.total_procesos
        : 0.0f;
    float retorno_prom = (stats.total_procesos > 0)
        ? stats.tiempo_retorno_total / (float)stats.total_procesos
        : 0.0f;
    float utilizacion = (stats.tiempo_total > 0) ?
        ((-1)*(stats.tiempo_total - stats.tiempo_espera_total) * 100.0f / stats.tiempo_total) : 0;
    

    printf("Procesos:             %d\n", stats.total_procesos);
    printf("Tiempo total:         %d\n", stats.tiempo_total);
    printf("Cambios de contexto:  %d\n", stats.cambios_contexto);
    printf("Espera promedio:      %.2f\n", espera_prom);
    printf("Retorno promedio:     %.2f\n", retorno_prom);
    printf("Utilización CPU:      %.2f%%\n", utilizacion);

    printf("\n┌─ Tabla de Procesos ─────────────────────────────────────────────────────────────┐\n");
    printf("│ PID │ Nombre         │ Estado     │ Espera  │ Retorno   │ Respuesta │ Desalojos │\n");
    printf("├─────┼────────────────┼────────────┼─────────┼───────────┤───────────┤───────────┤\n");
    
    
    for (int i = 0; i < num_procesos; i++) {
        printf("| %3d | %-12s   | %-10s | %6d  | %7d   | %9d | %9d |\n",
               procesos[i]->pid,
               procesos[i]->nombre,
               estado_a_texto(procesos[i]->estado),
               procesos[i]->tiempo_espera,
               procesos[i]->tiempo_retorno,
               procesos[i]->tiempo_respuesta,
               procesos[i]->contador_desalojos);
    }
    printf("├─────┼────────────────┼────────────┼─────────┼───────────┤───────────┤───────────┤\n");
}

/* imprimir_estadisticas: imprime métricas agregadas y una tabla con los
 * tiempos por proceso tras ejecutar un planificador.
 */

/* ==================== FUNCIONES DE INTEGRACION ==================== */

/*
 * init_procesos
 * Inicializa el modulo de procesos para uso desde el kernel.
 * El kernel la llama una sola vez al arrancar.
 */
void init_procesos(void) {
    reset_stats();
    printf("[PROC] Modulo de procesos inicializado.\n");
}

/*
 * menu_procesos
 * Menu interactivo del modulo de planificacion.
 * El kernel lo invoca como submenu desde su menu principal.
 * Antes era el main() de processes.c.
 */
void menu_procesos(void) {
    const int total_recursos[MAX_RECURSOS] = {10, 5, 7};
    const int recursos_p1[MAX_RECURSOS] = {2, 1, 1};
    const int recursos_p2[MAX_RECURSOS] = {3, 1, 2};
    const int recursos_p3[MAX_RECURSOS] = {2, 2, 1};
    const int recursos_p4[MAX_RECURSOS] = {1, 1, 2};

    PCB *procesos[MAX_PROCESOS];
    procesos[0] = crear_proceso(1, "Editor",     8,  0, 3, 0, recursos_p1);
    procesos[1] = crear_proceso(2, "Compilador", 4, -5, 1, 0, recursos_p2);
    procesos[2] = crear_proceso(3, "Navegador", 12,  5, 4, 0, recursos_p3);
    procesos[3] = crear_proceso(4, "Servidor",   6, -2, 2, 0, recursos_p4);

    int num_procesos = 4;
    int opcion;

    printf("\n╔═══════════════════════════════════════════════════════════╗\n");
    printf("║        MODULO DE PLANIFICACION DE PROCESOS                ║\n");
    printf("╚═══════════════════════════════════════════════════════════╝\n");

    do {
        printf("\n--- Menu de Planificacion ---\n");
        printf("1. FCFS\n");
        printf("2. Round Robin (quantum=%d)\n", QUANTUM);
        printf("3. SJF\n");
        printf("4. Prioridad\n");
        printf("5. CFS (Arbol Rojo-Negro)\n");
        printf("6. Ejecutar todos los algoritmos\n");
        printf("0. Volver al menu principal\n");
        printf("Seleccione: ");
        scanf("%d", &opcion);

        if (opcion >= 1 && opcion <= 5) {
            ReadyQueue cola;
            reset_stats();
            init_sistema_recursos(total_recursos);
            resetear_procesos(procesos, num_procesos);
            cargar_cola(&cola, procesos, num_procesos);
            llamar_planificador_por_id(opcion, &cola, procesos, num_procesos);
            imprimir_estadisticas(procesos, num_procesos);
        } else if (opcion == 6) {
            for (int algoritmo = 1; algoritmo <= 5; algoritmo++) {
                ReadyQueue cola;
                reset_stats();
                init_sistema_recursos(total_recursos);
                resetear_procesos(procesos, num_procesos);
                cargar_cola(&cola, procesos, num_procesos);
                llamar_planificador_por_id(algoritmo, &cola, procesos, num_procesos);
                imprimir_estadisticas(procesos, num_procesos);
            }
        } else if (opcion != 0) {
            printf("Opcion invalida.\n");
        }

    } while (opcion != 0);

    for (int i = 0; i < num_procesos; i++) {
        liberar_pcb(procesos[i]);
    }
}
