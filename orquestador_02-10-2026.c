#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/select.h>

#include <time.h> 

#include  "TrigramaM.h"

#define ARCHIVO "trigram.json"

struct nodo {
    int sockCliente;
    struct nodo *siguiente;
};

struct candidato {
    char llave[27];
    float score;
};

char *analisisFrecuencias();
void aceptarClientes(struct nodo **clientes, int *nClientes, int servidor);
void generadorSemillas(struct candidato *llaves, char semilla[], int N, int inicio);
void formatoSemillas(struct candidato *llaves, char cadena[], int n, int inicio);

int main(){
    int puerto = 6767; 
    struct nodo *clientes = NULL;
    int nClientes = 0;

    srand(time(NULL)); 

    int servidor = socket(AF_INET, SOCK_STREAM, 0);
    if(servidor < 0){ 
        printf("Error crendo el socket\n"); 
        return 1 ;
    }

    int opcion = 1;
    setsockopt(servidor, SOL_SOCKET, SO_REUSEADDR, &opcion, sizeof(opcion)); //solo para debugear pq siempre marca en ocupado cuando acaba, ignorenlo :p

    struct sockaddr_in direccion;
    memset(&direccion, 0, sizeof(direccion));
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = INADDR_ANY;
    direccion.sin_port = htons(puerto); 
    if(bind(servidor, (struct sockaddr *)&direccion, sizeof(direccion)) < 0){
        printf("Error haciendo el bind\n");
        return 1;
    }

    if(listen(servidor, 100) < 0){
        printf("Error al poner en escucha\n");
        return 1;
    }
    printf("Escuchando en %i\n", puerto );
   

    trigrama_main("corpus");
    //funcion de nava para el yeison

    aceptarClientes(&clientes, &nClientes, servidor);

    if(nClientes == 0){
        printf("No se conecto ningun cliente\n");
        close(servidor);
        return 0;
    }

    struct candidato llaves[nClientes * 100];
    char semilla[27];
    strcpy(semilla, analisisFrecuencias());
    strcpy(llaves[0].llave, semilla);
    generadorSemillas(llaves, semilla, nClientes * 100, 1);

    struct candidato ceamgu; //va a ser la variable que compara si ya le pegamos al gordo
    ceamgu.score = 0; 
    ceamgu.llave[0] = '\0';
        
    fd_set socketsClientes;
    int maxFD = 0;
    int activos = nClientes; 
    FD_ZERO(&socketsClientes);

    struct nodo *ptrTMP = clientes;
    char lote[2601];
    int indice = 0;
    for(int i = 0; i < nClientes; i++){
        
        formatoSemillas(llaves, lote, 99, indice);
        printf("Lote [%i-%i] -> fd %i | primera llave: %.26s\n", indice, indice + 99, ptrTMP->sockCliente, lote);
        indice += 100; 
        generadorSemillas(llaves, semilla, indice, indice - 100);
        if(indice >= nClientes * 100) indice = 0; 

        size_t bLeidos = sizeof(lote);
        size_t enviados = 0;
        int falloEnvio = 0;
        while(enviados < bLeidos){
            ssize_t resultado = send(ptrTMP->sockCliente, lote + enviados, bLeidos - enviados, MSG_NOSIGNAL);
            if(resultado <= 0){
                printf("Error al enviar lote de llaves %i\n", ptrTMP->sockCliente);
                close(ptrTMP->sockCliente);
                ptrTMP->sockCliente = -1;
                activos--;
                falloEnvio = 1;
                break;
            }
            enviados += resultado;
         }

        if(!falloEnvio){
            FD_SET(ptrTMP->sockCliente, &socketsClientes);
            if(ptrTMP->sockCliente > maxFD) maxFD = ptrTMP->sockCliente; 
        }
        ptrTMP = ptrTMP->siguiente;
    }
    
    indice = 0;
    while(ceamgu.score < 100 && activos > 0){ 

        fd_set lectura = socketsClientes; 
        int listos = select(maxFD + 1, &lectura, NULL, NULL, NULL);
        if(listos < 0){
            printf("Error en el select\n");
            break;
        }

        ptrTMP = clientes; 
        for(int i = 0; i < nClientes; i++){
            char buffer_recibo[36]; 
            char llave[27]; 
            char scoreTMP[6];       
            float scoreFloat = 0.0f;

            if(ptrTMP->sockCliente != -1 && FD_ISSET(ptrTMP->sockCliente, &lectura)){
                ssize_t recibidos = recv(ptrTMP->sockCliente, buffer_recibo, sizeof(buffer_recibo) - 1, 0); 
                if (recibidos > 0) {
                    buffer_recibo[recibidos] = '\0'; 

                    if (recibidos >= 26) { //solo se toma en cuenta si llego la llave completa
                        strncpy(llave, buffer_recibo, 26);
                        llave[26] = '\0'; 

                        if (recibidos > 26) {
                            strncpy(scoreTMP, &buffer_recibo[26], sizeof(scoreTMP) - 1);
                            scoreTMP[sizeof(scoreTMP) - 1] = '\0'; 
                            scoreFloat = strtof(scoreTMP, NULL); 
                        }

                        printf("fd %i respondio -> llave: %s | score: %.2f\n", ptrTMP->sockCliente, llave, scoreFloat);

                        if(scoreFloat > ceamgu.score){
                            strcpy(ceamgu.llave, llave);
                            strcpy(semilla, llave);
                            ceamgu.score = scoreFloat;
                            printf("Nueva mejor llave: %s | score: %.2f\n", ceamgu.llave, ceamgu.score);
                        }
                    } else {
                        printf("fd %i mando una respuesta incompleta (%zd bytes), se ignora\n", ptrTMP->sockCliente, recibidos);
                    }

                    if(scoreFloat < 95){
                        formatoSemillas(llaves, lote, 99, indice);
                        printf("Lote [%i-%i] -> fd %i | primera llave: %.26s\n", indice, indice + 99, ptrTMP->sockCliente, lote);

                        size_t bLeidos = sizeof(lote);
                        size_t enviados = 0;
                        int falloEnvio = 0;
                        while(enviados < bLeidos){
                            ssize_t resultado = send(ptrTMP->sockCliente, lote + enviados, bLeidos - enviados, MSG_NOSIGNAL);
                            if(resultado <= 0){
                                printf("Error al enviar lote de llaves %i\n", ptrTMP->sockCliente);
                                falloEnvio = 1;
                                break;
                            }
                            enviados += resultado;
                         }

                        if(!falloEnvio){
                            indice += 100; 
                            generadorSemillas(llaves, semilla, indice, indice - 100);
                            if(indice >= nClientes * 100) indice = 0; 
                        }else{
                            //si no se le pudo mandar, matamos al ciente y lo sacamos del set
                            FD_CLR(ptrTMP->sockCliente, &socketsClientes);
                            close(ptrTMP->sockCliente);
                            ptrTMP->sockCliente = -1;
                            activos--;
                        }
                    }
                } else {
                    printf("Cliente %i desconectado\n", ptrTMP->sockCliente);
                    FD_CLR(ptrTMP->sockCliente, &socketsClientes);
                    close(ptrTMP->sockCliente);
                    ptrTMP->sockCliente = -1;
                    activos--;
                }
            }
            ptrTMP = ptrTMP->siguiente; 
        }       

    }

    printf("Semilla obtenida: %s | score : %f\n", ceamgu.llave, ceamgu.score);

    
    ptrTMP = clientes;
    while(ptrTMP != NULL){
        struct nodo *siguiente = ptrTMP->siguiente;
        if(ptrTMP->sockCliente != -1) close(ptrTMP->sockCliente);
        free(ptrTMP);
        ptrTMP = siguiente;
    }
    close(servidor);

    return 0;
}

void formatoSemillas(struct candidato *llaves, char cadena[], int n, int inicio){
    cadena[0] = '\0';
    for(int i = inicio; i < n + inicio + 1; i++){
       strcat(cadena, llaves[i].llave); 
    }
}

void generadorSemillas(struct candidato *llaves, char semilla[], int N, int inicio){
    int intentos = 0;
    for(int i = N - 1; i >= inicio;){
 
        int origen = rand() % 26;
        int destino = rand() % 26;

        if(origen != destino){
            char llaveTMP[27]; 
            strcpy(llaveTMP, semilla);
            char letraTMP = llaveTMP[origen];
            llaveTMP[origen] = llaveTMP[destino];
            llaveTMP[destino] = letraTMP;

            int repetida = 0;
            for(int j = i + 1; j < N && !repetida; j++){
                if(strcmp(llaves[j].llave, llaveTMP) == 0) repetida = 1;
            }
            if(repetida && intentos < 1000){
                intentos++;  //solo hay 325 por eso el limite, para  intentar no repetir en cada bucle
                continue;
            }
            intentos = 0;

            strcpy(llaves[i].llave, llaveTMP);
            i--;
        }
    }
}

void aceptarClientes(struct nodo **clientes, int *nClientes, int servidor){
    struct nodo *temporal = *clientes;

    printf("Enter para dejar de aceptar clientes\n");

    while(1){
        fd_set lectura;

        FD_ZERO(&lectura);
        FD_SET(servidor, &lectura);
        FD_SET(STDIN_FILENO, &lectura);

        int maxFD = servidor;

        if(STDIN_FILENO > maxFD){
            maxFD = STDIN_FILENO;
        }

        int resultado = select(maxFD + 1, &lectura, NULL, NULL, NULL);
        if(resultado < 0){
            printf("Error en el select\n");
            break;
        }

        if(FD_ISSET(STDIN_FILENO, &lectura)){
            getchar();
            break;
        }

        if(FD_ISSET(servidor, &lectura)){
            int clienteTMP = accept(servidor, NULL, NULL);
            if(clienteTMP < 0){
                printf("Error al aceptar un cliente\n");
                continue;
            }


            FILE *archivo = fopen(ARCHIVO, "rb");
            if (archivo == NULL){
                printf("Error al enviar el archivo a fd %i\n", clienteTMP);
                close(clienteTMP);
                continue;
            }else{
                char buffer[4096];
                size_t bLeidos;
                int falloEnvio = 0;

                while(!falloEnvio && (bLeidos = fread(buffer, 1 , sizeof(buffer), archivo)) > 0){
                    size_t enviados = 0;
                    while(enviados < bLeidos){
                        ssize_t resultado = send(clienteTMP, buffer + enviados, bLeidos - enviados, MSG_NOSIGNAL);
                        
                        if(resultado <= 0){
                            printf("Error al enviar el archivo a fd %i\n", clienteTMP);
                            falloEnvio = 1; 
                            break;
                        }
                        enviados += resultado;
                    }
                }
                fclose(archivo);

                if(falloEnvio){
                    close(clienteTMP);
                    continue; 
                }

                if(send(clienteTMP, "\0", 1, MSG_NOSIGNAL) <= 0){
                    printf("Error al enviar el fin de archivo a fd %i\n", clienteTMP);
                    close(clienteTMP);
                    continue;
                }

                char listo;
                ssize_t confirmacion = recv(clienteTMP, &listo, 1, 0);
                if(confirmacion <= 0){
                    printf("El cliente %i no confirmo la recepcion del archivo\n", clienteTMP);
                    close(clienteTMP);
                    continue;
                }
            }

            struct nodo *nuevo = malloc(sizeof(struct nodo));
            if(nuevo == NULL){
                printf("Error al reservar memoria\n");
                close(clienteTMP);
                continue;
            }

            nuevo->sockCliente = clienteTMP;
            nuevo->siguiente = NULL;
            if(temporal == NULL){
                *clientes = nuevo;
                temporal = nuevo;
            }else{
                temporal->siguiente = nuevo;
                temporal = nuevo;
            }

            (*nClientes)++;
            
        }
    }
}

char *analisisFrecuencias(){
    return "eaosinrdlctupmbgfvyhqjzxkw";
}
