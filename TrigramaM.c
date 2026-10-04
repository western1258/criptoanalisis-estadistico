#include <stdio.h>

#define TAM 26

int trigrama_main(const char *ruta_corpus) {

    char *nombre_salida = "trigram.json";

    FILE *entrada = fopen(ruta_corpus, "rb");
    if (entrada == NULL) {
        printf("ERROS al abrir el CORPUS, %s\n", ruta_corpus);
        return 1;
    }

    static long conteo[TAM][TAM][TAM];    
    long total = 0;

    int a = -1, b = -1;
    int c;

    while ((c = fgetc(entrada)) != EOF) {
        if(c < 'A' || c > 'Z') continue;
        int x = c - 'A';

        if (a >= 0) {
            conteo[a][b][x]++;
            total++;
        }
        a = b;
        b = x;
    }
    fclose(entrada);

    if (total == 0) return 1;

    FILE *json = fopen(nombre_salida, "w");
    if (json == NULL) {
        printf("No se pudo crear %s\n", nombre_salida);
        return 1;
    }
 
    fprintf(json, "{\n");
    int primero = 1;
    
    while (1) {
        long maximo = 0;              /* mayor numero de apariciones encontrado */
        int mi = 0, mj = 0, mk = 0;   /* las 3 letras de ese trigrama */
 
        for (int i = 0; i < TAM; i++) {
            for (int j = 0; j < TAM; j++) {
                for (int k = 0; k < TAM; k++) {
                    if (conteo[i][j][k] > maximo) {
                        maximo = conteo[i][j][k];
                        mi = i;
                        mj = j;
                        mk = k;
                    }
                }
            }
        }
 
        if (maximo == 0)              /* ya no queda ningun trigrama */
            break;
 
        if (!primero)
            fprintf(json, ",\n");     /* coma entre elementos */
        primero = 0;
 
        fprintf(json, "  \"%c%c%c\": %.6f",
        'A' + mi, 'A' + mj, 'A' + mk,
        maximo * 100.0 / total);
 
        conteo[mi][mj][mk] = 0;       /* ya se escribio: se tacha */
    }

    fprintf(json, "\n}\n");
    fclose(json);
    
    return 0;
}
