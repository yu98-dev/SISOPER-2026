#include <stdio.h>
#include <stdbool.h>

#define MEMORY_SIZE 1024
#define PARTITIONS 4
#define PART_SIZE MEMORY_SIZE/PARTITIONS /*Calcula el tamaño de cada particion*/

typedef struct particionamiento1
{
    int process_id;
    int size;
    bool occupied;

} Partition;

/*REPRESENTACION LOGICA DEL PARTICIONAMIENTO DE MEMORIA*/
Partition memory[PARTITIONS];

void initialize_mem()
{
    for(int i=0; i<PARTITIONS; i++)
    {
        memory[i].process_id=-1;
        memory[i].size=0;
        memory[i].occupied = false;
    }
}

void print_mem()
{
    printf("\nEstado de asignación de memoria: ");
    for (int i = 0; i < PARTITIONS; i++)
    {
        if (memory[i].occupied)
        {
            printf("particion %d, proceso %d (%d bytes)", i, memory[i].process_id,memory[i].size);
        }
        else
        {
            printf("Particion %d está libre",i);
        }
    }
    
}

void allocate_mem(int proc_id, int size)
{
    if(size > PART_SIZE)
    {
        printf("\nNo puede reservar ese tamaño de memoria");
        return;
    }
    //busqueda de un bloque de memoria libre - secuencial:
    for (int i = 0; i < PARTITIONS; i++)
    {
        if(! memory[i].occupied)
        {
            memory[i].process_id=proc_id;
            memory[i].size=size;
            memory[i].occupied=true;
            return;
        }
    }
    //si al final del recorrido no encuentra bloque libre, retorna
    return;    
}

void free_mem(int proc_id)
{
    for (int i = 0; i < PARTITIONS; i++)
    {
        if(memory[i].process_id==proc_id)
        {
            memory[i].process_id=-1;
            memory[i].size=0;
            memory[i].occupied=false;
            return;
        }
    }
}

int main()
{
    //PRUEBA:
    //INICIALIZAMOS:
    initialize_mem();
    //ALOJAMOS 3 PROCESOS:
    allocate_mem(1,400);
    allocate_mem(2,100);
    allocate_mem(3,250);
    allocate_mem(4, 45);
    //IMPRIMIMOS:
    print_mem();
    //LIBERAMOS UN PROCESO:
    free_mem(3);
    //IMPRIMIMOS DE NUEVO:
    print_mem();
    return 0;
}
