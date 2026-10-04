#include <stdio.h>

#define TAM 27
#define INDICE_EÑE 14


static const char *LETRAS[TAM] = {
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "\xC3\x91", 
    "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z"
};


int indice_letra(int c, FILE *archivo) {
 
    if (c >= 'A' && c <= 'Z') {
        int i = c - 'A';
        if (i >= INDICE_EÑE)
            i++;                        
        return i;
    }
    // la Ñ en Latin-1
    if (c == 0xD1)
        return INDICE_EÑE;
    // la Ñ en UTF-8
    if (c == 0xC3) {                    
        int d = fgetc(archivo);
        if (d == 0x91)     
            return INDICE_EÑE;
    }
 
    return -1;
}


int main(int argc, char *argv[]) {

    char *nombre_salida = "trigram.json";

    FILE *entrada = fopen(argv[1], "rb");
    if (entrada == NULL) {
        printf("ERROS al abrir el CORPUS, %s\n", argv[1]);
        return 1;
    }

    static long conteo[TAM][TAM][TAM];    
    long total = 0;

    int a = -1, b = -1;
    int c;

    while ((c = fgetc(entrada)) != EOF) {
        int x = indice_letra(c, entrada);
        if (x < 0) continue;

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
 
        fprintf(json, "  \"%s%s%s\": %.6f",
                LETRAS[mi], LETRAS[mj], LETRAS[mk],
                maximo * 100.0 / total);
 
        conteo[mi][mj][mk] = 0;       /* ya se escribio: se tacha */
    }

    fprintf(json, "\n}\n");
    fclose(json);
    
    return 0;
}
