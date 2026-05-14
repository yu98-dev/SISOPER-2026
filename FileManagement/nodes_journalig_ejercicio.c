#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define MAX_LINEA 256
#define MAX_LOG 10
#define BLOCK_SIZE 1024

/* =========================
   ESTRUCTURAS INODE
   ========================= */

typedef struct {
    int inode_number;
    int size;
    int block;
    int used;
} Inode;

/* =========================
   ESTRUCTURA JOURNAL
   ========================= */

typedef struct {
    char operation[32];
    int inode;
    int committed;
} JournalEntry;

/* =========================
   VARIABLES GLOBALES
   ========================= */

char disk_block[BLOCK_SIZE];

Inode inode_table[4];

JournalEntry journal[MAX_LOG];

int log_index = 0;

/* =========================
   FUNCIONES JOURNAL
   ========================= */

void log_start(const char *op, int inode)
{
    strcpy(journal[log_index].operation, op);
    journal[log_index].inode = inode;
    journal[log_index].committed = 0;

    log_index++;
}

void log_commit()
{
    journal[log_index - 1].committed = 1;
}

void show_journal()
{
    int i;

    printf("\n===== JOURNAL =====\n");

    for(i = 0; i < log_index; i++)
    {
        printf("Operacion: %s | inode: %d | committed: %d\n",
               journal[i].operation,
               journal[i].inode,
               journal[i].committed);
    }
}

/* =========================
   FUNCIONES INODE
   ========================= */

void fs_create(int inode_num)
{
    log_start("CREATE", inode_num);

    inode_table[inode_num].inode_number = inode_num;
    inode_table[inode_num].size = 0;
    inode_table[inode_num].block = inode_num;
    inode_table[inode_num].used = 1;

    log_commit();
}

void fs_write(int inode_num, const char *text)
{
    log_start("WRITE", inode_num);

    strcat(disk_block, text);

    inode_table[inode_num].size = strlen(disk_block);

    log_commit();
}

void fs_close(int inode_num)
{
    log_start("CLOSE", inode_num);

    log_commit();
}

/* =========================
   FUNCIONES ORIGINALES
   ========================= */

int contar_palabras(const char *linea);
void convertir_mayusculas(char *linea);

int main()
{
    FILE *archivo_entrada;
    FILE *archivo_salida;

    char buffer[MAX_LINEA];

    int total_lineas = 0;
    int total_palabras = 0;

    /* LIMPIAR BLOQUE DE DISCO */
    disk_block[0] = '\0';

    /* =========================
       CREAR INODE
       ========================= */

    fs_create(0);

    /* =========================
       ABRIR ARCHIVOS
       ========================= */

    archivo_entrada = fopen("entrada.txt", "r");

    if(archivo_entrada == NULL)
    {
        perror("ARCHIVO NO ENCONTRADO");
        return EXIT_FAILURE;
    }

    archivo_salida = fopen("salida.txt", "w");

    if(archivo_salida == NULL)
    {
        perror("ERROR AL CREAR");

        fclose(archivo_entrada);

        return EXIT_FAILURE;
    }

    /* =========================
       PROCESAMIENTO
       ========================= */

    while(fgets(buffer, MAX_LINEA, archivo_entrada) != NULL)
    {
        total_lineas++;

        total_palabras += contar_palabras(buffer);

        convertir_mayusculas(buffer);

        fputs(buffer, archivo_salida);

        /* =========================
           ESCRIBIR EN "DISCO"
           ========================= */

        fs_write(0, buffer);
    }

    /* =========================
       ESTADISTICAS
       ========================= */

    fprintf(archivo_salida, "\n--ESTADISTICAS--\n");

    fprintf(archivo_salida,
            "Total lineas: %d\n",
            total_lineas);

    fprintf(archivo_salida,
            "Total palabras: %d\n",
            total_palabras);

    /* =========================
       CERRAR INODE
       ========================= */

    fs_close(0);

    /* =========================
       CERRAR ARCHIVOS
       ========================= */

    fclose(archivo_entrada);
    fclose(archivo_salida);

    /* =========================
       MOSTRAR INFORMACION
       ========================= */

    printf("Proceso completado.\n");

    printf("\n===== INFORMACION INODE =====\n");

    printf("Inode: %d\n",
           inode_table[0].inode_number);

    printf("Size: %d bytes\n",
           inode_table[0].size);

    printf("Block: %d\n",
           inode_table[0].block);

    printf("Used: %d\n",
           inode_table[0].used);

    show_journal();

    return 0;
}

/* =========================
   CONTAR PALABRAS
   ========================= */

int contar_palabras(const char *linea)
{
    int contador = 0;
    int en_palabra = 0;

    for(int i = 0; linea[i] != '\0'; i++)
    {
        if(!isspace((unsigned char)linea[i]))
        {
            if(!en_palabra)
            {
                en_palabra = 1;
                contador++;
            }
        }
        else
        {
            en_palabra = 0;
        }
    }

    return contador;
}

/* =========================
   CONVERTIR MAYUSCULAS
   ========================= */

void convertir_mayusculas(char *linea)
{
    for(int i = 0; linea[i] != '\0'; i++)
    {
        linea[i] = (char)toupper((unsigned char)linea[i]);
    }
}