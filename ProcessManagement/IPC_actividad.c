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
            // TODO: abrir FIFO para lectura
            // TODO: leer datos, copiar en memoria compartida y calcular a*b
            // TODO: usar malloc para crear un arreglo de resultados
            // TODO: imprimir resultados parciales
            //men->a, mem->b, mem->listo
            exit(0);
        } else {
            pid_cons2 = fork();
            if (pid_cons2 == 0) {
                // Segundo consumidor: calcula suma
                // TODO: abrir FIFO para lectura
                // TODO: leer datos, copiar en memoria compartida y calcular a+b
                // TODO: usar malloc para almacenar resultados sumados
                // TODO: imprimir resultados parciales
                exit(0);
            }
        }
    }

    // TODO: el proceso padre debe esperar a que todos los hijos terminen
    // TODO: liberar memoria compartida y eliminar el FIFO

    return 0;
}