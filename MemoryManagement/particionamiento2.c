/*#include <stdio.h>
#include <stdbool.h>

#define MEMORY_SIZE 1024
#define MAX_BLOCKS 20

typedef struct {
    int start;
    int size;
    int process_id;
    bool free;
} Block;

Block blocks[MAX_BLOCKS];
int block_count = 1;

void initialize_memory() {
    blocks[0].start = 0;
    blocks[0].size = MEMORY_SIZE;
    blocks[0].free = true;
    blocks[0].process_id = -1;
}

void print_memory() {
    printf("\nEstado de memoria:\n");
    for (int i = 0; i < block_count; i++) {
        printf("Bloque %d | Inicio: %d | Tamaño: %d | %s\n",
               i,
               blocks[i].start,
               blocks[i].size,
               blocks[i].free ? "Libre" : "Ocupado");
    }
}

void allocate(int pid, int size) {
    for (int i = 0; i < block_count; i++) {
        if (blocks[i].free && blocks[i].size >= size) {

            // Crear nuevo bloque si sobra espacio
            if (blocks[i].size > size) {
                for (int j = block_count; j > i; j--)
                    blocks[j] = blocks[j - 1];

                blocks[i + 1].start = blocks[i].start + size;
                blocks[i + 1].size = blocks[i].size - size;
                blocks[i + 1].free = true;
                blocks[i + 1].process_id = -1;

                blocks[i].size = size;
                block_count++;
            }

            blocks[i].free = false;
            blocks[i].process_id = pid;
            return;
        }
    }

    printf("No hay espacio suficiente.\n");
}

void allocate_bestfit(int pid, int size) {

    //establezco los valores para comparar
    int best_index=-1;
    int best_index_size=MEMORY_SIZE;

    if (block_count==1)
    {
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

void free_block(int pid) {
    for (int i = 0; i < block_count; i++) {
        if (blocks[i].process_id == pid) {
            blocks[i].free = true;
            blocks[i].process_id = -1;
        }
    }
}

void compact_memory() {
    int new_start = 0;

    for (int i = 0; i < block_count; i++) {
        if (!blocks[i].free) {
            blocks[i].start = new_start;
            new_start += blocks[i].size;
        }
    }

    // Crear bloque libre consolidado
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

int main() {
    printf("SIMULACION BEST FIT\n");
    initialize_memory();
    print_memory();

    allocate_bestfit(1, 200);
    allocate_bestfit(2, 300);
    allocate_bestfit(3, 100);
    allocate_bestfit(4, 50);

    print_memory();

    free_block(2);
    free_block(3);
    print_memory();

    allocate_bestfit(5, 75);

    print_memory();

    compact_memory();
    print_memory();

    return 0;
}*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// malloc -> crea un bloque de memoria con el tamaño solicitado y le da informacion basura
// calloc -> crea un bloque de memoria con el tamaño solicitado y lo inicializa a cero
// realloc -> cambia el tamaño de un bloque de memoria previamente asignado

// Variables globales (Segmento de Datos)
int global_var = 42; //minimo(?
static int static_global_var = 100; //tamaño total de la memoria

// Función para mostrar direcciones de memoria de distintos segmentos
void mostrar_segmentos() {
    int local_var = 10; // Variable local (Pila)
    static int static_local_var = 20; // Variable estática local (Datos)
    int *heap_var = (int*) malloc(sizeof(int)); // Variable en Heap

    printf("\n--- Direcciones de memoria ---\n");
    printf("Variable global: %p\n", (void*)&global_var);
    printf("Variable estática global: %p\n", (void*)&static_global_var);
    printf("Variable local: %p\n", (void*)&local_var);
    printf("Variable estática local: %p\n", (void*)&static_local_var);
    printf("Variable en Heap: %p\n", (void*)heap_var);

    free(heap_var); // Liberar memoria asignada en Heap
}

// Función para mostrar el consumo de memoria del proceso usando /proc/self/status
void mostrar_consumo_memoria() {
    char comando[50];
    snprintf(comando, sizeof(comando), "cat /proc/%d/status | grep -E 'VmSize|VmRSS'", getpid());
    printf("\n--- Consumo de Memoria ---\n");
    system(comando);
}

// Función para asignar memoria dinámica
void asignar_memoria_dinamica(size_t size) {
    int *arr = (int*) malloc(size * sizeof(int));
    if (arr == NULL) {
        printf("Error al asignar memoria.\n");
        return;
    }

    // Llenar el arreglo con datos
    for (size_t i = 0; i < size; i++) {
        arr[i] = i;
    }

    printf("Se asignaron %zu enteros en memoria dinámica.\n", size);
    mostrar_consumo_memoria();

    // Esperar entrada del usuario antes de liberar memoria
    printf("Presiona ENTER para liberar la memoria...");
    getchar();
    free(arr);
    printf("Memoria liberada.\n");
    mostrar_consumo_memoria();
}

//función para asignar memoria dinámica sin liberar (mejor caso)
    void asignar_memoria_dinamica_best(size_t size) {
    int *arr = (int*) malloc(size * sizeof(int));
    if (arr == NULL) {
        printf("Error al asignar memoria.\n");
        return;
    }

    // Llenar el arreglo con datos
    for (size_t i = 0; i < size; i++) {
        arr[i] = i;
    }
    }

    // Función para asignar memoria dinámica sin liberar (peor caso)
    void asignar_memoria_dinamica_worst(size_t size) {
    int *arr = (int*) malloc(size * sizeof(int));
    if (arr == NULL) {
        printf("Error al asignar memoria.\n");
        return;
    }

    // Llenar el arreglo con datos
    for (size_t i = 0; i < size; i++) {
        arr[i] = i;
    }
    }
    
// Menú interactivo
void menu() {
    int opcion;
    size_t size;

    do {
        printf("\n--- Menú de Gestión de Memoria ---\n");
        printf("1. Mostrar direcciones de memoria\n");
        printf("2. Mostrar consumo de memoria\n");
        printf("3. Asignar memoria dinámica\n");
        printf("4. Salir\n");
        printf("Seleccione una opción: ");
        scanf("%d", &opcion);
        getchar(); // Limpiar buffer de entrada

        switch (opcion) {
            case 1:
                mostrar_segmentos();
                break;
            case 2:
                mostrar_consumo_memoria();
                break;
            case 3:
                printf("Ingrese la cantidad de enteros a asignar en memoria: ");
                scanf("%zu", &size);
                getchar(); // Limpiar buffer de entrada
                asignar_memoria_dinamica(size);
                break;
            case 4:
                printf("Saliendo...\n");
                break;
            default:
                printf("Opción no válida.\n");
        }
    } while (opcion != 4);
}

int main() {
    menu();
    return 0;
}