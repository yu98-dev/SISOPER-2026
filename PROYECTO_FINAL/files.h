/*
 * files.h
 * Declaraciones publicas del modulo de sistema de archivos.
 * Incluir este header en cualquier .c que necesite operar archivos.
 */

#ifndef FILES_H
#define FILES_H

/* ── Constantes ──────────────────────────────────────────────── */
#define MAX_FILES    20
#define MAX_NAME     50
#define MAX_CONTENT 256
#define MAX_PATH    300  /* MAX_NAME + len("archivos/") + margen */

/* ── Estructura ──────────────────────────────────────────────── */
typedef struct {
    char nombre[MAX_NAME];
    char contenido[MAX_CONTENT];
    int  abierto;   /* 1 = abierto/bloqueado, 0 = disponible */
    int  usado;     /* 1 = slot ocupado,       0 = libre      */
} Archivo;

/* ── Funciones ───────────────────────────────────────────────── */
void init_filesystem(void);
void crear_archivo(char nombre[]);
void abrir_archivo(char nombre[]);
void escribir_archivo(char nombre[], char texto[]);
void leer_archivo(char nombre[]);
void cerrar_archivo(char nombre[]);
void eliminar_archivo(char nombre[]);
void listar_archivos(void);

/*
 * archivo_disponible()
 * Permite que el kernel consulte el estado de un archivo
 * sin acceder directamente a disco[].
 * Retorna:
 *   0 = no existe
 *   1 = existe y esta cerrado (disponible)
 *   2 = existe pero esta abierto (bloqueado)
 */
int archivo_disponible(char nombre[]);

#endif /* FILES_H */
