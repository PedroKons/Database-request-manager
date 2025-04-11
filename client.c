// client.c
// Cliente que envia comandos para o servidor via memória compartilhada

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "db.h"

int main() {
    // Acessa a memória compartilhada já criada pelo servidor
    int shmid = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
    if (shmid == -1) {
        perror("Erro ao acessar memória compartilhada");
        return 1;
    }

    // Mapeia a memória compartilhada no espaço do processo
    SharedMemory *shared_data = (SharedMemory*) shmat(shmid, NULL, 0);

    char input[MAX_MSG];
    while (1) {
        printf("Digite um comando (INSERT id=1 name='Joao', LIST, DELETE id=1, EXIT): ");
        fgets(input, sizeof(input), stdin);                      // Lê entrada do usuário
        input[strcspn(input, "\n")] = '\0';                      // Remove o \n do final

        while (shared_data->ready == 1);                         // Espera o servidor processar o anterior

        strcpy(shared_data->message, input);                     // Copia comando para a memória
        shared_data->ready = 1;                                  // Sinaliza nova mensagem

        if (strcmp(input, "EXIT") == 0)
            break;
    }

    shmdt(shared_data); // Desanexa a memória compartilhada
    return 0;
}
