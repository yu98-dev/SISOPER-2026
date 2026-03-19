#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>

/* Estructura PCB (Process Control Block) */
typedef struct {
    pid_t pid;
    pid_t ppid;
    char nombre[50];
    int estado;  /* 0: Creado, 1: Ejecutando, 2: Finalizado */
    int cpu_time;
} PCB;

void mostrar_pcb(PCB *pcb, const char *tipo) {
    printf("\n========================================\n");
    printf("PCB - %s\n", tipo);
    printf("========================================\n");
    printf("PID:       %d\n", pcb->pid);
    printf("PPID:      %d\n", pcb->ppid);
    printf("Nombre:    %s\n", pcb->nombre);
    printf("Estado:    %d\n", pcb->estado);
    printf("CPU Time:  %d ms\n", pcb->cpu_time);
    printf("========================================\n\n");
}

int main() {
    PCB proceso_padre;
    PCB proceso_hijo1;
    PCB proceso_hijo2;
    
    /* Inicializar PCB del proceso padre */
    proceso_padre.pid = getpid();
    proceso_padre.ppid = getppid();
    snprintf(proceso_padre.nombre, sizeof(proceso_padre.nombre), "Proceso Padre");
    proceso_padre.estado = 1;
    proceso_padre.cpu_time = 0;
    
    mostrar_pcb(&proceso_padre, "PROCESO PADRE");
    
    /* Crear primer subproceso */
    pid_t pid1 = fork();
    
    if (pid1 == 0) {
        /* Código ejecutado por el primer hijo */
        proceso_hijo1.pid = getpid();
        proceso_hijo1.ppid = getppid();
        snprintf(proceso_hijo1.nombre, sizeof(proceso_hijo1.nombre), "Hijo 1");
        proceso_hijo1.estado = 1;
        proceso_hijo1.cpu_time = 100;
        
        mostrar_pcb(&proceso_hijo1, "SUBPROCESO 1");
        
        sleep(2);
        exit(0);
    } 
    else if (pid1 > 0) {
        /* Crear segundo subproceso desde el padre */
        pid_t pid2 = fork();
        
        if (pid2 == 0) {
            /* Código ejecutado por el segundo hijo */
            proceso_hijo2.pid = getpid();
            proceso_hijo2.ppid = getppid();
            snprintf(proceso_hijo2.nombre, sizeof(proceso_hijo2.nombre), "Hijo 2");
            proceso_hijo2.estado = 1;
            proceso_hijo2.cpu_time = 150;
            
            mostrar_pcb(&proceso_hijo2, "SUBPROCESO 2");
            
            sleep(3);
            exit(0);
        } 
        else if (pid2 > 0) {
            /* El padre espera a ambos hijos */
            printf("\nPadre esperando a sus hijos...\n");
            wait(NULL);
            wait(NULL);
            
            printf("\nTodos los subprocesos han finalizado.\n");
            mostrar_pcb(&proceso_padre, "PROCESO PADRE (Final)");
        }
    } 
    else {
        perror("Error en fork");
        exit(1);
    }
    
    return 0;
}