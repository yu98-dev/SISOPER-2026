#include <stdio.h>
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
    initialize_memory();

    allocate(1, 200);
    allocate(2, 300);
    allocate(3, 100);

    print_memory();

    free_block(2);
    print_memory();

    compact_memory();
    print_memory();

    return 0;
}