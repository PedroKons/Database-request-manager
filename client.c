#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "db.h"

int main() {
    int shmid = shmget(SHM_KEY, sizeof(SharedMemory), 0666);
    if (shmid == -1) {
        perror("Erro ao acessar memória compartilhada");
        return 1;
    }

    SharedMemory *shared_data = (SharedMemory*) shmat(shmid, NULL, 0);

    char input[MAX_MSG];
    while (1) {
        printf("Digite um comando (INSERT id=1 name='Joao', LIST, DELETE id=1, EXIT): ");
        fgets(input, sizeof(input), stdin);
        input[strcspn(input, "\n")] = '\0';

        while (shared_data->ready == 1);

        strcpy(shared_data->message, input);
        shared_data->ready = 1;

        if (strcmp(input, "EXIT") == 0)
            break;
    }

    shmdt(shared_data);
    return 0;
}
