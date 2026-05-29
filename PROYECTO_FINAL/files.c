/* files.c
   Sistema de archivos sobre disco real.
   Los archivos se guardan en la subcarpeta archivos/
   La tabla disco[] mantiene el estado (abierto/cerrado)
   en memoria para controlar el lock por proceso.
*/

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include "files.h"

/* ── Directorio base del filesystem ─────────────────────────── */
#define DIR_ARCHIVOS "archivos"

/* ── Tabla de estado en memoria ──────────────────────────────── */
/* Guarda nombre y estado (abierto/cerrado) de cada archivo.
   El contenido vive en disco, no aqui.                          */
Archivo disco[MAX_FILES];

/* ── Construye la ruta completa del archivo ──────────────────── */
static void ruta(char dest[], const char nombre[]) {
    snprintf(dest, MAX_PATH, "%s/%s", DIR_ARCHIVOS, nombre);
}

/* ── Busca un archivo en la tabla por nombre ─────────────────── */
static int buscar(const char nombre[]) {
    int i;
    for(i = 0; i < MAX_FILES; i++)
        if(disco[i].usado &&
           strncmp(disco[i].nombre, nombre, MAX_NAME) == 0)
            return i;
    return -1;
}

/* ════════════════════════════════════════════════════════════════
   INICIALIZAR
   ════════════════════════════════════════════════════════════════ */

void init_filesystem() {
    int i;

    /* Limpiar tabla en memoria */
    for(i = 0; i < MAX_FILES; i++) {
        disco[i].usado   = 0;
        disco[i].abierto = 0;
        disco[i].nombre[0] = '\0';
    }

    /* Crear carpeta archivos/ si no existe */
    mkdir(DIR_ARCHIVOS, 0755);

    printf("[FS] Filesystem inicializado en './%s/'\n", DIR_ARCHIVOS);
}

/* ════════════════════════════════════════════════════════════════
   CREAR ARCHIVO
   ════════════════════════════════════════════════════════════════ */

void crear_archivo(char nombre[]) {
    int i;
    char path[MAX_PATH];

    /* Verificar que no exista ya en la tabla */
    if(buscar(nombre) >= 0) {
        printf("Error: archivo '%s' ya existe.\n", nombre);
        return;
    }

    /* Buscar slot libre en la tabla */
    for(i = 0; i < MAX_FILES; i++) {
        if(!disco[i].usado) {
            /* Crear el archivo en disco */
            ruta(path, nombre);
            FILE *f = fopen(path, "w");
            if(!f) {
                printf("Error: no se pudo crear '%s' en disco.\n", nombre);
                return;
            }
            fclose(f);

            /* Registrar en tabla */
            strncpy(disco[i].nombre, nombre, MAX_NAME - 1);
            disco[i].nombre[MAX_NAME - 1] = '\0';
            disco[i].abierto = 0;
            disco[i].usado   = 1;

            printf("Archivo '%s' creado en %s/\n", nombre, DIR_ARCHIVOS);
            return;
        }
    }
    printf("Error: tabla de archivos llena.\n");
}

/* ════════════════════════════════════════════════════════════════
   ABRIR ARCHIVO
   ════════════════════════════════════════════════════════════════ */

void abrir_archivo(char nombre[]) {
    int idx = buscar(nombre);
    if(idx < 0) {
        printf("Error: archivo '%s' no encontrado.\n", nombre);
        return;
    }
    if(disco[idx].abierto) {
        printf("Error: '%s' esta bloqueado, en uso por otro proceso.\n",
               nombre);
        return;
    }
    disco[idx].abierto = 1;
    printf("Archivo '%s' abierto.\n", nombre);
}

/* ════════════════════════════════════════════════════════════════
   ESCRIBIR ARCHIVO
   ════════════════════════════════════════════════════════════════ */

void escribir_archivo(char nombre[], char texto[]) {
    int idx = buscar(nombre);
    char path[MAX_PATH];

    if(idx < 0) {
        printf("Error: archivo '%s' no encontrado.\n", nombre);
        return;
    }
    if(!disco[idx].abierto) {
        printf("Error: '%s' no esta abierto. Abralo primero.\n", nombre);
        return;
    }

    /* Escribir en disco — modo "w" sobreescribe */
    ruta(path, nombre);
    FILE *f = fopen(path, "w");
    if(!f) {
        printf("Error: no se pudo escribir en '%s'.\n", nombre);
        return;
    }
    fprintf(f, "%s", texto);
    fclose(f);

    printf("Escrito en '%s'.\n", nombre);
}

/* ════════════════════════════════════════════════════════════════
   LEER ARCHIVO
   ════════════════════════════════════════════════════════════════ */

void leer_archivo(char nombre[]) {
    int idx = buscar(nombre);
    char path[MAX_PATH];
    char linea[MAX_CONTENT];

    if(idx < 0) {
        printf("Error: archivo '%s' no encontrado.\n", nombre);
        return;
    }
    if(!disco[idx].abierto) {
        printf("Error: '%s' no esta abierto. Abralo primero.\n", nombre);
        return;
    }

    /* Leer desde disco */
    ruta(path, nombre);
    FILE *f = fopen(path, "r");
    if(!f) {
        printf("Error: no se pudo leer '%s' desde disco.\n", nombre);
        return;
    }

    printf("Contenido de '%s':\n", nombre);
    while(fgets(linea, sizeof(linea), f))
        printf("%s", linea);
    printf("\n");
    fclose(f);
}

/* ════════════════════════════════════════════════════════════════
   CERRAR ARCHIVO
   ════════════════════════════════════════════════════════════════ */

void cerrar_archivo(char nombre[]) {
    int idx = buscar(nombre);
    if(idx < 0) {
        printf("Error: archivo '%s' no encontrado.\n", nombre);
        return;
    }
    if(!disco[idx].abierto) {
        printf("Aviso: '%s' ya estaba cerrado.\n", nombre);
        return;
    }
    disco[idx].abierto = 0;
    printf("Archivo '%s' cerrado.\n", nombre);
}

/* ════════════════════════════════════════════════════════════════
   ELIMINAR ARCHIVO
   ════════════════════════════════════════════════════════════════ */

void eliminar_archivo(char nombre[]) {
    int idx = buscar(nombre);
    char path[MAX_PATH];

    if(idx < 0) {
        printf("Error: archivo '%s' no encontrado.\n", nombre);
        return;
    }
    if(disco[idx].abierto) {
        printf("Error: no se puede eliminar '%s', esta abierto.\n", nombre);
        return;
    }

    /* Eliminar del disco */
    ruta(path, nombre);
    if(remove(path) != 0)
        printf("Aviso: no se pudo eliminar '%s' del disco.\n", nombre);

    /* Limpiar entrada de la tabla */
    disco[idx].usado     = 0;
    disco[idx].abierto   = 0;
    disco[idx].nombre[0] = '\0';

    printf("Archivo '%s' eliminado.\n", nombre);
}

/* ════════════════════════════════════════════════════════════════
   CONSULTAR ESTADO — sin exponer disco[]
   ════════════════════════════════════════════════════════════════ */

int archivo_disponible(char nombre[]) {
    int idx = buscar(nombre);
    if(idx < 0)          return 0;  /* no existe        */
    if(disco[idx].abierto) return 2;  /* existe, bloqueado */
    return 1;                         /* existe, libre     */
}

/* ════════════════════════════════════════════════════════════════
   LISTAR ARCHIVOS
   ════════════════════════════════════════════════════════════════ */

void listar_archivos() {
    int i, hay = 0;

    printf("\nArchivos en sistema (./%s/):\n", DIR_ARCHIVOS);
    for(i = 0; i < MAX_FILES; i++) {
        if(disco[i].usado) {
            printf("  [%s] %s\n",
                   disco[i].abierto ? "ABIERTO" : "cerrado",
                   disco[i].nombre);
            hay = 1;
        }
    }
    if(!hay) printf("  (ninguno)\n");
}
