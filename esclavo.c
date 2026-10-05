#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <math.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define TOTAL_TRIGRAMAS 17576
#define MAX_TEXTO       100000
#define MAX_JSON        (8 * 1024 * 1024)
#define LARGO_LLAVE     26
#define LLAVES_POR_LOTE 100
#define BYTES_LOTE      (LLAVES_POR_LOTE * LARGO_LLAVE + 1)

float         modelo_log[TOTAL_TRIGRAMAS];
unsigned char texto_cifrado[MAX_TEXTO];
unsigned char texto_descifrado[MAX_TEXTO];
char          lote[BYTES_LOTE];
char          json_recibido[MAX_JSON];
int           largo_texto = 0;

double        ref_mejor_prom = 0.0;
double        ref_peor_prom  = 0.0;

char *leer_archivo(const char *nombre) {
    FILE *archivo = fopen(nombre, "rb");
    if (archivo == NULL) {
        printf("No se pudo abrir %s\n", nombre);
        return NULL;
    }
    fseek(archivo, 0, SEEK_END);
    long tamano = ftell(archivo);
    if (tamano < 0) {
        fclose(archivo);
        return NULL;
    }
    rewind(archivo);

    char *contenido = malloc(tamano + 1);
    if (contenido == NULL) {
        fclose(archivo);
        return NULL;
    }
    size_t leidos = fread(contenido, 1, tamano, archivo);
    contenido[leidos] = '\0';
    fclose(archivo);
    return contenido;
}

int cargar_texto_cifrado(const char *nombre) {
    char *contenido = leer_archivo(nombre);
    if (contenido == NULL) {
        return -1;
    }

    largo_texto = 0;
    for (int i = 0; contenido[i] != '\0'; i++) {

        if ((contenido[i] >= 'A' && contenido[i] <= 'Z') ||
            (contenido[i] >= 'a' && contenido[i] <= 'z')) {
            if (largo_texto >= MAX_TEXTO) {
                printf("El texto cifrado es muy largo (maximo %d)\n", MAX_TEXTO);
                free(contenido);
                return -1;
            }
            texto_cifrado[largo_texto] = (unsigned char)(toupper(contenido[i]) - 'A');
            largo_texto++;
        }
    }
    free(contenido);

    if (largo_texto < 3) {
        printf("El texto cifrado es muy corto\n");
        return -1;
    }
    printf("[+] Texto cifrado cargado: %d letras\n", largo_texto);
    return 0;
}

int recibir_todo(int sock, void *buffer, size_t cantidad) {
    size_t leidos = 0;
    while (leidos < cantidad) {
        ssize_t r = recv(sock, (char *)buffer + leidos, cantidad - leidos, 0);
        if (r < 0 && errno == EINTR) {
            continue;
        }
        if (r <= 0) {
            return -1;
        }
        leidos += (size_t)r;
    }
    return 0;
}

int enviar_todo(int sock, const void *buffer, size_t cantidad) {
    size_t enviados = 0;
    while (enviados < cantidad) {
        ssize_t r = send(sock, (const char *)buffer + enviados, cantidad - enviados, 0);
        if (r < 0 && errno == EINTR) {
            continue;
        }
        if (r <= 0) {
            return -1;
        }
        enviados += (size_t)r;
    }
    return 0;
}

int recibir_json(int sock) {
    size_t total = 0;
    while (1) {
        if (total >= MAX_JSON - 1) {
            printf("El json es mas grande de lo esperado\n");
            return -1;
        }
        ssize_t r = recv(sock, json_recibido + total, MAX_JSON - 1 - total, 0);
        if (r < 0 && errno == EINTR) {
            continue;
        }
        if (r <= 0) {
            printf("Se corto la conexion recibiendo el json\n");
            return -1;
        }

        char *fin = memchr(json_recibido + total, '\0', (size_t)r);
        total += (size_t)r;
        if (fin != NULL) {
            printf("[+] JSON recibido (%zu bytes)\n", (size_t)(fin - json_recibido));
            return 0;
        }
    }
}

int recibir_lote(int sock) {
    if (recibir_todo(sock, lote, BYTES_LOTE) < 0) {
        return -1;
    }
    if (lote[BYTES_LOTE - 1] != '\0') {
        fprintf(stderr, "Lote sin terminador valido\n");
        return -1;
    }
    for (int i = 0; i < LLAVES_POR_LOTE; i++) {
        unsigned int usadas = 0;
        for (int j = 0; j < LARGO_LLAVE; j++) {
            unsigned char letra = (unsigned char)toupper((unsigned char)lote[i * LARGO_LLAVE + j]);
            if (letra < 'A' || letra > 'Z' || (usadas & (1u << (letra - 'A')))) {
                fprintf(stderr, "Llave invalida en el lote\n");
                return -1;
            }
            usadas |= 1u << (letra - 'A');
        }
    }
    return 0;
}

int enviar_resultado(int sock, const char *llave, float score) {
    char mensaje[LARGO_LLAVE + 32];

    memcpy(mensaje, llave, LARGO_LLAVE);
    int largo_score = snprintf(mensaje + LARGO_LLAVE, sizeof(mensaje) - LARGO_LLAVE, "%05.2f", score);

    return enviar_todo(sock, mensaje, LARGO_LLAVE + (size_t)largo_score);
}

int cargar_modelo(const char *contenido) {
    static double valores[TOTAL_TRIGRAMAS];
    int encontrados = 0;

    memset(valores, 0, sizeof(valores));
    const char *limite_contenido = contenido + strlen(contenido);

    const char *p = contenido;
    while (*p != '\0') {
        if (limite_contenido - p >= 5 && *p == '"' && isalpha((unsigned char)p[1]) && isalpha((unsigned char)p[2])
            && isalpha((unsigned char)p[3]) && p[4] == '"') {

            int indice = (toupper(p[1]) - 'A') * 676
                       + (toupper(p[2]) - 'A') * 26
                       + (toupper(p[3]) - 'A');

            const char *q = p + 5;
            while (*q == ' ' || *q == '\t' || *q == '\n' || *q == '\r') {
                q++;
            }
            if (*q == ':') {
                q++;
                char *fin;
                double valor = strtod(q, &fin);
                if (fin != q && isfinite(valor) && valor >= 0.0) {
                    valores[indice] = valor;
                    encontrados++;
                    p = fin;
                    continue;
                }
            }
        }
        p++;
    }

    if (encontrados == 0) {
        printf("No se encontraron trigramas en el json (revisar el formato)\n");
        return -1;
    }

    double suma = 0.0;
    double minimo = 1e300;
    for (int i = 0; i < TOTAL_TRIGRAMAS; i++) {
        suma += valores[i];
        if (valores[i] > 0.0 && valores[i] < minimo) {
            minimo = valores[i];
        }
    }

    if (!isfinite(suma) || suma <= 0.0) {
        fprintf(stderr, "El modelo no contiene valores positivos validos\n");
        return -1;
    }

    double B;
    if (suma > 1.5) {
        printf("[*] El json trae conteos, se convierten a probabilidades\n");
        B = suma;
        for (int i = 0; i < TOTAL_TRIGRAMAS; i++) {
            valores[i] = valores[i] / suma;
        }
    } else {

        B = 1.0 / minimo;
    }

    double prob_minima = 1.0 / (10.0 * B);
    double entropia = 0.0;
    for (int i = 0; i < TOTAL_TRIGRAMAS; i++) {
        double prob = valores[i];
        if (prob > 0.0) {
            entropia -= prob * log2(prob);
        } else {
            prob = prob_minima;
        }
        modelo_log[i] = (float)log2(prob);
    }

    ref_mejor_prom = -entropia;
    ref_peor_prom  = log2(prob_minima);
    printf("[*] Referencias por trigrama: 100%% = %.3f, 0%% = %.3f\n",
           ref_mejor_prom, ref_peor_prom);

    printf("[+] Modelo cargado: %d trigramas, B = %.0f\n", encontrados, B);
    return 0;
}

float score_a_porcentaje(float score_log) {
    double trigramas = (double)(largo_texto - 2);
    double promedio = (double)score_log / trigramas;
    double porcentaje = 100.0 * (promedio - ref_peor_prom) / (ref_mejor_prom - ref_peor_prom);

    if (porcentaje < 0.01) porcentaje = 0.01;
    if (porcentaje > 99.99) porcentaje = 99.99;
    return (float)porcentaje;
}

float evaluar_llave(const char *llave) {
    unsigned char tabla[26];

    for (int i = 0; i < LARGO_LLAVE; i++) {
        tabla[i] = (unsigned char)(((llave[i] & 31) + 25) % 26);
    }

    for (int i = 0; i < largo_texto; i++) {
        texto_descifrado[i] = tabla[texto_cifrado[i]];
    }

    double score = 0.0;
    int limite = largo_texto - 2;
    for (int i = 0; i < limite; i++) {
        int indice = texto_descifrado[i] * 676
                   + texto_descifrado[i + 1] * 26
                   + texto_descifrado[i + 2];
        score += modelo_log[indice];
    }
    return (float)score;
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);
    const char *ip            = (argc > 1) ? argv[1] : "127.0.0.1";
    int puerto                = (argc > 2) ? atoi(argv[2]) : 67;
    if (puerto < 1 || puerto > 65535) {
        fprintf(stderr, "Puerto invalido\n");
        return 1;
    }
    const char *archivo_texto = (argc > 3) ? argv[3] : "cifrado.txt";

    if (cargar_texto_cifrado(archivo_texto) < 0) {
        return 1;
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("Error creando el socket\n");
        return 1;
    }

    struct sockaddr_in direccion_master;
    memset(&direccion_master, 0, sizeof(direccion_master));
    direccion_master.sin_family = AF_INET;
    direccion_master.sin_port = htons((uint16_t)puerto);
    if (inet_pton(AF_INET, ip, &direccion_master.sin_addr) <= 0) {
        printf("IP invalida: %s\n", ip);
        close(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&direccion_master, sizeof(direccion_master)) < 0) {
        printf("Error conectando al Master\n");
        close(sock);
        return 1;
    }
    printf("[+] Conectado al Master %s:%d\n", ip, puerto);

    if (recibir_json(sock) < 0) {
        close(sock);
        return 1;
    }
    if (cargar_modelo(json_recibido) < 0) {
        close(sock);
        return 1;
    }
    char listo = 1;
    if (enviar_todo(sock, &listo, 1) < 0) {
        printf("No se pudo confirmar al Master\n");
        close(sock);
        return 1;
    }
    printf("[+] Confirmacion enviada, esperando lotes...\n");

    while (1) {
        if (recibir_lote(sock) < 0) {
            printf("El Master cerro la conexion, terminamos\n");
            break;
        }
        printf("Lote recibido: evaluando %d llaves...\n", LLAVES_POR_LOTE);
        fflush(stdout);

        float mejor_score = -INFINITY;
        int mejor_posicion = 0;
        for (int i = 0; i < LLAVES_POR_LOTE; i++) {
            float score = evaluar_llave(lote + i * LARGO_LLAVE);
            if (score > mejor_score) {
                mejor_score = score;
                mejor_posicion = i;
            }
        }

        float porcentaje = score_a_porcentaje(mejor_score);
        printf("Lote evaluado: mejor score = %.2f\n", porcentaje);
        fflush(stdout);
        if (enviar_resultado(sock, lote + mejor_posicion * LARGO_LLAVE, porcentaje) < 0) {
            printf("Error mandando el resultado\n");
            break;
        }
    }

    close(sock);
    return 0;
}
