#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <sys/wait.h>

#define FIFO_FILE "canal_fifo"
#define SHM_KEY 1234
#define BUFFER_SIZE 128

typedef struct {
    int a;
    int b;
    int resultado_mul;
    int resultado_sum;
    int listo;
} DatosCompartidos;

int main() {
    int shm_id;
    DatosCompartidos *mem;
    pid_t pid_prod, pid_cons1, pid_cons2;

    // Crear memoria compartida
    shm_id = shmget(SHM_KEY, sizeof(DatosCompartidos), IPC_CREAT | 0666);
    if (shm_id < 0) {
        perror("Error creando memoria compartida");
        exit(1);
    }

    mem = (DatosCompartidos *) shmat(shm_id, NULL, 0);
    if (mem == (DatosCompartidos *) -1) {
        perror("Error asociando memoria compartida");
        exit(1);
    }

    // Crear FIFO si no existe
    mkfifo(FIFO_FILE, 0666);

    pid_prod = fork();

    if (pid_prod == 0) {
        // --- Lógica de Productor con Interbloqueo ---
if (pid_prod == 0) {
    int fd = open(FIFO_FILE, O_WRONLY);
    
    for (int i = 1; i <= 5; i++) {
        // 1. El productor espera a que el consumidor limpie la memoria (Interbloqueo inicial)
        while (mem->listo == 1); 

        char buffer[BUFFER_SIZE];
        snprintf(buffer, BUFFER_SIZE, "%d %d", i, i + 2);
        
        printf("[Productor] Intentando escribir en FIFO...\n");
        // 2. Escribe en el FIFO. Si el buffer del sistema se llena, aquí se bloquea.
        write(fd, buffer, strlen(buffer) + 1); 
        
        // 3. El flag que el consumidor necesita para empezar a leer se activa DESPUÉS
        mem->listo = 1; 
        printf("[Productor] Flag 'listo' activado.\n");
    }
    exit(0);
}

        // Proceso Productor
        int fd = open(FIFO_FILE, O_WRONLY);
        if (fd < 0) {
            perror("Error abriendo FIFO para escritura");
            exit(1);
        }

        for (int i = 1; i <= 5; i++) {
            int a = i, b = i + 2;
            char buffer[BUFFER_SIZE];
            snprintf(buffer, BUFFER_SIZE, "%d %d", a, b);
            write(fd, buffer, strlen(buffer) + 1);
            printf("[Productor] Enviado: %s\n", buffer);
            sleep(1);
        }

        close(fd);
        exit(0);
    } else {
        // Crear dos consumidores
        pid_cons1 = fork();
        if (pid_cons1 == 0) {
            // Primer consumidor: calcula multiplicación
            int fd = open(FIFO_FILE, O_RDONLY);
            if (fd < 0) {
                perror("Error abriendo FIFO para lectura (cons1)");
                exit(1);
            }

            // Usar malloc para almacenar resultados de multiplicaciones
            int *resultados_mul = (int *)malloc(5 * sizeof(int));
            if (resultados_mul == NULL) {
                perror("Error asignando memoria");
                close(fd);
                exit(1);
            }

            char buffer[BUFFER_SIZE];
            int idx = 0; 

            // Leer datos del FIFO y calcular multiplicación
            while (read(fd, buffer, BUFFER_SIZE) > 0 && idx < 5) {
                int a, b;
                sscanf(buffer, "%d %d", &a, &b);
                
                // Copiar a memoria compartida
                mem->a = a;
                mem->b = b;
                mem->listo = 1;

                // Calcular multiplicación
                int resultado = a * b;
                resultados_mul[idx] = resultado;
                
                printf("[Consumidor 1] Leído: %d, %d -> Multiplicación: %d\n", a, b, resultado);
                idx++;
            }

            close(fd);
            mem->resultado_mul = resultados_mul[idx - 1]; // Guardar última multiplicación

            // Imprimir todos los resultados
            printf("[Consumidor 1] Resultados almacenados:\n");
            for (int i = 0; i < idx; i++) {
                printf("  %d * %d = %d\n", i+1, i+3, resultados_mul[i]);
            }

            free(resultados_mul);
            exit(0);
        } else {
            pid_cons2 = fork();
            if (pid_cons2 == 0) {
                // Segundo consumidor: calcula suma
                int fd = open(FIFO_FILE, O_RDONLY);
                if (fd < 0) {
                    perror("Error abriendo FIFO para lectura (cons2)");
                    exit(1);
                }

                // Usar malloc para almacenar resultados de sumas
                int *resultados_sum = (int *)malloc(5 * sizeof(int));
                if (resultados_sum == NULL) {
                    perror("Error asignando memoria");
                    close(fd);
                    exit(1);
                }

                char buffer[BUFFER_SIZE];
                int idx = 0;

                // Leer datos del FIFO y calcular suma
                while (read(fd, buffer, BUFFER_SIZE) > 0 && idx < 5) {
                    int a, b;
                    sscanf(buffer, "%d %d", &a, &b);
                    
                    // Copiar a memoria compartida
                    mem->a = a;
                    mem->b = b;
                    mem->listo = 1;

                    // Calcular suma
                    int resultado = a + b;
                    resultados_sum[idx] = resultado;
                    
                    printf("[Consumidor 2] Leído: %d, %d -> Suma: %d\n", a, b, resultado);
                    idx++;
                }

                close(fd);
                mem->resultado_sum = resultados_sum[idx - 1]; // Guardar última suma

                // Imprimir todos los resultados
                printf("[Consumidor 2] Resultados almacenados:\n");
                for (int i = 0; i < idx; i++) {
                    printf("  %d + %d = %d\n", i+1, i+3, resultados_sum[i]);
                }

                free(resultados_sum);
                exit(0);
            }
        }
    }

    // El proceso padre espera a que todos los hijos terminen
    waitpid(pid_prod, NULL, 0);
    waitpid(pid_cons1, NULL, 0);
    waitpid(pid_cons2, NULL, 0);

    printf("\n[Padre] Todos los procesos han terminado\n");
    printf("[Padre] Resultados finales en memoria compartida:\n");
    printf("  Última multiplicación: %d\n", mem->resultado_mul);
    printf("  Última suma: %d\n", mem->resultado_sum);

    // Liberar memoria compartida
    shmdt((void *)mem);
    shmctl(shm_id, IPC_RMID, NULL);

    // Eliminar el FIFO
    unlink(FIFO_FILE);

    printf("[Padre] Recursos liberados\n");

    return 0;
}