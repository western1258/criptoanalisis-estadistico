#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

#include "AnalisisFrec.h"

#define TAM 26

static char semilla[TAM + 1];


/*
 * Convierte caracteres UTF-8 del español a ASCII.
 *
 * Á á -> A
 * É é -> E
 * Í í -> I
 * Ó ó -> O
 * Ú ú -> U
 * Ü ü -> U
 * Ñ ñ -> N
 */
static char convertirUTF8(unsigned char c1, unsigned char c2)
{
    if (c1 != 0xC3)
        return '\0';

    switch (c2) {
        case 0x81:
        case 0xA1:
            return 'A';

        case 0x89:
        case 0xA9:
            return 'E';

        case 0x8D:
        case 0xAD:
            return 'I';

        case 0x93:
        case 0xB3:
            return 'O';

        case 0x9A:
        case 0xBA:
            return 'U';

        case 0x9C:
        case 0xBC:
            return 'U';

        case 0x91:
        case 0xB1:
            return 'N';
    }

    return '\0';
}


/*
 * Lee un archivo y escribe únicamente A-Z
 * en el corpus.
 */
static void sanitizarArchivo(FILE *entrada, FILE *corpus)
{
    int c;

    while ((c = fgetc(entrada)) != EOF) {

        /*
         * Mayúsculas ASCII.
         */
        if (c >= 'A' && c <= 'Z') {
            fputc(c, corpus);
            continue;
        }

        /*
         * Minúsculas ASCII.
         */
        if (c >= 'a' && c <= 'z') {
            fputc(c - 'a' + 'A', corpus);
            continue;
        }

        /*
         * Caracteres UTF-8.
         */
        if ((unsigned char)c == 0xC3) {

            int siguiente = fgetc(entrada);

            if (siguiente == EOF)
                break;

            char convertido = convertirUTF8(
                (unsigned char)c,
                (unsigned char)siguiente
            );

            if (convertido != '\0')
                fputc(convertido, corpus);
        }
    }
}


/*
 * Genera corpus.txt a partir de todos los archivos
 * normales encontrados dentro del directorio.
 */
static int generarCorpus(const char *directorio)
{
    DIR *dir = opendir(directorio);

    if (dir == NULL) {
        printf("ERROR: no se pudo abrir el directorio: %s\n",
               directorio);
        return 1;
    }

    FILE *corpus = fopen("corpus.txt", "w");

    if (corpus == NULL) {
        printf("ERROR: no se pudo crear corpus.txt\n");
        closedir(dir);
        return 1;
    }

    struct dirent *entrada;

    while ((entrada = readdir(dir)) != NULL) {

        if (strcmp(entrada->d_name, ".") == 0 ||
            strcmp(entrada->d_name, "..") == 0)
            continue;

        char ruta[1024];

        snprintf(
            ruta,
            sizeof(ruta),
            "%s/%s",
            directorio,
            entrada->d_name
        );

        struct stat info;

        if (stat(ruta, &info) != 0)
            continue;

        if (!S_ISREG(info.st_mode))
            continue;

        FILE *archivo = fopen(ruta, "rb");

        if (archivo == NULL) {
            printf("AVISO: no se pudo abrir %s\n", ruta);
            continue;
        }

        printf("Procesando: %s\n", ruta);

        sanitizarArchivo(archivo, corpus);

        fclose(archivo);
    }

    fclose(corpus);
    closedir(dir);

    return 0;
}


/*
 * Analiza la frecuencia de las letras del texto cifrado
 * y genera corpus.txt a partir del directorio indicado.
 *
 * Devuelve una cadena de 26 letras ordenadas de mayor
 * a menor frecuencia.
 */
char *analisisFrecuencias(const char *ruta_cifrado,
                          const char *directorio_corpus)
{
    long frecuencia[TAM] = {0};
    int usadas[TAM] = {0};

    /*
     * Abrir texto cifrado.
     */
    FILE *cifrado = fopen(ruta_cifrado, "rb");

    if (cifrado == NULL) {
        printf("ERROR: no se pudo abrir el texto cifrado: %s\n",
               ruta_cifrado);

        semilla[0] = '\0';

        return semilla;
    }

    /*
     * Contar frecuencia de las letras.
     */
    int c;

    while ((c = fgetc(cifrado)) != EOF) {

        if (c >= 'A' && c <= 'Z') {
            frecuencia[c - 'A']++;
        }
        else if (c >= 'a' && c <= 'z') {
            frecuencia[c - 'a']++;
        }
    }

    fclose(cifrado);


    /*
     * Generar corpus.txt.
     */
    if (generarCorpus(directorio_corpus) != 0) {
        semilla[0] = '\0';
        return semilla;
    }


    /*
     * Ordenar las letras según su frecuencia.
     */
    for (int posicion = 0; posicion < TAM; posicion++) {

        long mayor = -1;
        int letra = -1;

        for (int i = 0; i < TAM; i++) {

            if (!usadas[i] && frecuencia[i] > mayor) {
                mayor = frecuencia[i];
                letra = i;
            }
        }

        if (letra == -1)
            break;

        semilla[posicion] = 'A' + letra;
        usadas[letra] = 1;
    }


    /*
     * Completar las letras que no aparecieron
     * en el texto cifrado.
     */
    int posicion = 0;

    while (posicion < TAM && semilla[posicion] != '\0')
        posicion++;

    for (int i = 0; i < TAM && posicion < TAM; i++) {

        if (!usadas[i]) {
            semilla[posicion] = 'A' + i;
            usadas[i] = 1;
            posicion++;
        }
    }

    semilla[TAM] = '\0';

    return semilla;
}
