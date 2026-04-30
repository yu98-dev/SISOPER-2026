#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>

#define MAX_LINEA 256

int contar_palabras(const char *linea);
void convertir_mayusculas(char *linea);

int main() {
    FILE *archivo_entrada;
    FILE *archivo_salida;

    char buffer[MAX_LINEA];
    int total_lineas = 0;
    int total_palabras = 0;

    /* TODO: Abrir archivo de entrada en modo lectura */
    archivo_entrada=fopen("entrada.txt", "r");

    /* TODO: Verificar si el archivo se abrió correctamente */
    if(archivo_entrada==NULL){
        perror("ARCHIVO NO ENCONTRADO");
        return EXIT_FAILURE;
    }

    /* TODO: Abrir archivo de salida en modo escritura */
    archivo_salida=fopen("salida.txt", "w");

    /* TODO: Verificar si el archivo de salida se abrió correctamente */

    if(archivo_salida==NULL){
        perror("ERROR AL CREAR");
        fclose(archivo_entrada);
        return EXIT_FAILURE; }

    while (fgets(buffer, MAX_LINEA, archivo_entrada) != NULL) {
        total_lineas++;

        /* TODO: Contar palabras en la línea y acumular */
        total_palabras += contar_palabras(buffer);

        /* TODO: Convertir la línea a mayúsculas */
        convertir_mayusculas(buffer);

        /* TODO: Escribir la línea transformada en el archivo de salida */
        fputs(buffer, archivo_salida);
    }

    /* TODO: Escribir estadísticas en el archivo de salida */
    fprintf(archivo_salida, "\n--ESTADISTICAS--\n");
    fprintf(archivo_salida, "total lineas: %d\n", total_lineas);    
    fprintf(archivo_salida, "total palabras: %d\n", total_palabras); 


    /* TODO: Cerrar ambos archivos */
    fclose(archivo_entrada);
    fclose(archivo_salida);

    printf("Proceso completado.\n");

    return 0;
}

int contar_palabras(const char *linea) {
    int contador = 0;
    int en_palabra = 0;

    /* TODO: Implementar lógica */
    for(int i=0; linea[i] != '\0'; i++){
        if(!isspace((unsigned char)linea[i])){
            if(!en_palabra){
                en_palabra=1;
                contador++;
            }
        }
        else{
            en_palabra=0;
        }

    }

    return contador;
}

void convertir_mayusculas(char *linea) {
      /* TODO: Implementar conversión */
    for(int i=0; linea[i] != '\0'; i++){
        linea[i]=(char)toupper((unsigned char) linea[i]);}
  
}