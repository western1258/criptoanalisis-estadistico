#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/select.h>

#include <time.h> 

struct nodo {
    int sockCliente;
    struct nodo* siguiente;
};

struct candidato {
    char llave[27];
    float score;
};

char *analisisFrecuencias();
void aceptarClientes(struct nodo **clientes, int *nClientes, int servidor);
void generadorSemillas(struct candidato *llaves, char semilla[], int N, int inicio);

int main(){
    int puerto = 67;
    struct nodo *clientes = NULL;
    int nClientes = 0;
    int servidor = socket(AF_INET, SOCK_STREAM, 0);
    if(servidor < 0){ 
        printf("Error crendo el socket\n"); 
        return 1 ;
    }

    struct sockaddr_in direccion;
    direccion.sin_family = AF_INET;
    direccion.sin_addr.s_addr = INADDR_ANY;
    direccion.sin_port = htons(puerto); //htons convierte a orden de bytes para la red big-endian
    if(bind(servidor, (struct sockaddr *)&direccion, sizeof(direccion)) < 0){
        printf("Error haciendo el bind\n");
        return 1;
    }

    if(listen(servidor, 100) < 0){
        printf("Error al poner en escucha\n");
        return 1;
    }
    printf("Escuchando en %i\n", puerto );
    
    aceptarClientes(&clientes, &nClientes, servidor);


    struct candidato llaves[nClientes * 200];
    char semilla[27];
    strcpy(semilla, analisisFrecuencias());
    strcpy(llaves[0].llave, semilla);
    generadorSemillas(llaves, semilla, 99, 1);

    struct candidato ceamgu; //va a ser la variable que compara si ya le pegamos al gordo
    ceamgu.score = 0; 

    return 0;
}

void generadorSemillas(struct candidato *llaves, char semilla[], int N, int inicio){
    for(int i = inicio; i < N+inicio;){
        srand(time(NULL));

        int origen = rand() % 26;
        int destino = rand() % 26;

        if(origen != destino){
            char llaveTMP[26];
            strcpy(llaveTMP, semilla);
            char letraTMP = llaveTMP[origen];
            llaveTMP[origen] = llaveTMP[destino];
            llaveTMP[destino] = letraTMP;
            strcpy(llaves[i].llave, llaveTMP);
            i++;
        }
    } //las llaves se pueden repetir pero elias me dijo q no me aguite 
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

