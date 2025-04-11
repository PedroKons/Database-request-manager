// db.h
// Cabeçalho com definições compartilhadas entre cliente e servidor

#ifndef DB_H
#define DB_H

#define MAX_NAME 50           // Tamanho máximo do nome de um registro
#define MAX_MSG 256           // Tamanho máximo de uma mensagem/comando
#define SHM_KEY 1234          // Chave da memória compartilhada (IPC)
#define QUEUE_SIZE 10         // Tamanho da fila de tarefas no servidor
#define THREAD_POOL_SIZE 4    // Número de threads fixas no servidor

// Estrutura que representa um registro no banco
typedef struct {
    int id;                  // Identificador do registro
    char name[MAX_NAME];     // Nome do registro
} Record;

// Representa uma tarefa de comando recebida
typedef struct {
    char command[MAX_MSG];   // Comando enviado (INSERT, DELETE, etc.)
} Task;

// Fila circular de tarefas para o servidor
typedef struct {
    Task queue[QUEUE_SIZE];
    int front;
    int rear;
    int count;
} TaskQueue;

// Estrutura da memória compartilhada entre cliente e servidor
typedef struct {
    char message[MAX_MSG];   // Mensagem enviada pelo cliente
    int ready;               // 1 = nova mensagem pronta, 0 = lida
} SharedMemory;

#endif
