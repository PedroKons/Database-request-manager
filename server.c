// server.c
// Gerenciador de requisições com 4 threads fixas, usando fila, mutex e memória compartilhada

#include <stdio.h>      // Funções de entrada e saída
#include <stdlib.h>     // Funções de controle de memória e processo
#include <string.h>     // Manipulação de strings
#include <pthread.h>    // Threads POSIX
#include <unistd.h>     // Funções sleep/usleep
#include <sys/ipc.h>    // IPC: comunicação entre processos
#include <sys/shm.h>    // Memória compartilhada
#include "db.h"         // Cabeçalho com definições comuns

// Declara mutex e condição para proteger a fila de tarefas
pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;

// Mutex para controlar acesso ao arquivo db.txt
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

// Fila de tarefas global
TaskQueue task_queue;

// Ponteiro para acessar a memória compartilhada
SharedMemory *shared_data;

// Adiciona uma tarefa na fila de comandos
void enqueue(Task t) {
    pthread_mutex_lock(&queue_mutex);             // Bloqueia o acesso à fila
    if (task_queue.count < QUEUE_SIZE) {          // Se ainda há espaço na fila
        task_queue.queue[task_queue.rear] = t;    // Adiciona a tarefa no final
        task_queue.rear = (task_queue.rear + 1) % QUEUE_SIZE; // Avança a posição circular
        task_queue.count++;                       // Aumenta contador de tarefas
        pthread_cond_signal(&queue_cond);         // Acorda uma thread que estiver esperando
    }
    pthread_mutex_unlock(&queue_mutex);           // Libera a fila
}

// Remove uma tarefa da fila
Task dequeue() {
    pthread_mutex_lock(&queue_mutex);             // Bloqueia a fila
    while (task_queue.count == 0) {               // Se a fila estiver vazia
        pthread_cond_wait(&queue_cond, &queue_mutex); // Espera uma nova tarefa
    }
    Task t = task_queue.queue[task_queue.front];  // Pega a tarefa do início da fila
    task_queue.front = (task_queue.front + 1) % QUEUE_SIZE; // Avança circularmente
    task_queue.count--;                           // Diminui contador
    pthread_mutex_unlock(&queue_mutex);           // Libera a fila
    return t;                                     // Retorna tarefa para a thread
}

// Insere um registro no banco (db.txt)
void insert_record(Record r) {
    pthread_mutex_lock(&file_mutex);              // Bloqueia acesso ao arquivo
    FILE *fp = fopen("db.txt", "ab");             // Abre arquivo para adicionar (append binário)
    fwrite(&r, sizeof(Record), 1, fp);            // Escreve o registro no arquivo
    fclose(fp);                                   // Fecha o arquivo
    pthread_mutex_unlock(&file_mutex);            // Libera acesso ao arquivo
}

// Deleta um registro com ID específico
void delete_record(int id) {
    pthread_mutex_lock(&file_mutex);              // Bloqueia arquivo
    FILE *fp = fopen("db.txt", "rb");             // Abre o banco para leitura
    FILE *tmp = fopen("tmp.txt", "wb");           // Cria arquivo temporário
    Record r;

    while (fread(&r, sizeof(Record), 1, fp)) {    // Lê registros
        if (r.id != id)                           // Se não for o ID a ser deletado
            fwrite(&r, sizeof(Record), 1, tmp);   // Copia para o novo arquivo
    }

    fclose(fp);                                   // Fecha original
    fclose(tmp);                                  // Fecha temporário
    remove("db.txt");                             // Remove arquivo antigo
    rename("tmp.txt", "db.txt");                  // Renomeia temporário como principal
    pthread_mutex_unlock(&file_mutex);            // Libera acesso ao arquivo
}

// Lista todos os registros do banco
void list_records() {
    pthread_mutex_lock(&file_mutex);              // Bloqueia acesso ao arquivo
    FILE *fp = fopen("db.txt", "rb");             // Abre arquivo para leitura
    Record r;
    while (fread(&r, sizeof(Record), 1, fp)) {    // Lê cada registro
        printf("ID: %d, Name: %s\n", r.id, r.name); // Imprime no terminal
    }
    fclose(fp);                                   // Fecha o arquivo
    pthread_mutex_unlock(&file_mutex);            // Libera o arquivo
}

// Função executada por cada thread fixa
void* thread_worker(void *arg) {
    while (1) {
        Task task = dequeue();                    // Pega tarefa da fila
        printf("[Thread] Processando: %s\n", task.command);

        // Processa comando INSERT
        if (strncmp(task.command, "INSERT", 6) == 0) {
            Record r;
            sscanf(task.command, "INSERT id=%d name='%[^']'", &r.id, r.name);
            insert_record(r);
        }

        // Processa comando DELETE
        else if (strncmp(task.command, "DELETE", 6) == 0) {
            int id;
            sscanf(task.command, "DELETE id=%d", &id);
            delete_record(id);
        }

        // Processa comando LIST
        else if (strncmp(task.command, "LIST", 4) == 0) {
            list_records();
        }
    }
    return NULL;
}

// Função principal do servidor
int main() {
    // Cria segmento de memória compartilhada
    int shmid = shmget(SHM_KEY, sizeof(SharedMemory), IPC_CREAT | 0666);
    shared_data = (SharedMemory*) shmat(shmid, NULL, 0); // Mapeia a memória
    shared_data->ready = 0;                              // Marca como livre

    // Inicializa a fila de tarefas
    task_queue.front = 0;
    task_queue.rear = 0;
    task_queue.count = 0;

    // Cria as 4 threads fixas
    pthread_t threads[THREAD_POOL_SIZE]; //Aqui identificamos as threads 
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_create(&threads[i], NULL, thread_worker, NULL); //Criando as threads NULL onde eu passaria os valores para os atributos
    }

    printf("[Servidor] Aguardando comandos do cliente...\n");

    // Loop principal para ler comandos da memória compartilhada
    while (1) {
        if (shared_data->ready == 1) {                   // Se há comando novo
            if (strcmp(shared_data->message, "EXIT") == 0) // Comando de saída
                break;

            Task t;
            strcpy(t.command, shared_data->message);     // Copia comando
            enqueue(t);                                  // Adiciona à fila
            shared_data->ready = 0;                      // Marca como processado
        }
        usleep(10000); // Espera 10ms para evitar uso excessivo de CPU
    }

    // Libera e remove memória compartilhada
    shmdt(shared_data);
    shmctl(shmid, IPC_RMID, NULL);
    return 0;
}
