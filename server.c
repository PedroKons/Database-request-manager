#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include "db.h"

pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
pthread_mutex_t file_mutex = PTHREAD_MUTEX_INITIALIZER;

TaskQueue task_queue;
SharedMemory *shared_data;

void enqueue(Task t) {
    pthread_mutex_lock(&queue_mutex);
    if (task_queue.count < QUEUE_SIZE) {
        task_queue.queue[task_queue.rear] = t;
        task_queue.rear = (task_queue.rear + 1) % QUEUE_SIZE;
        task_queue.count++;
        pthread_cond_signal(&queue_cond);
    }
    pthread_mutex_unlock(&queue_mutex);
}

Task dequeue() {
    pthread_mutex_lock(&queue_mutex);
    while (task_queue.count == 0) {
        pthread_cond_wait(&queue_cond, &queue_mutex);
    }
    Task t = task_queue.queue[task_queue.front];
    task_queue.front = (task_queue.front + 1) % QUEUE_SIZE;
    task_queue.count--;
    pthread_mutex_unlock(&queue_mutex);
    return t;
}

void insert_record(Record r) {
    pthread_mutex_lock(&file_mutex);
    FILE *fp = fopen("db.txt", "ab");
    fwrite(&r, sizeof(Record), 1, fp);
    fclose(fp);
    pthread_mutex_unlock(&file_mutex);
}

void delete_record(int id) {
    pthread_mutex_lock(&file_mutex);
    FILE *fp = fopen("db.txt", "rb");
    FILE *tmp = fopen("tmp.txt", "wb");
    Record r;

    while (fread(&r, sizeof(Record), 1, fp)) {
        if (r.id != id)
            fwrite(&r, sizeof(Record), 1, tmp);
    }

    fclose(fp);
    fclose(tmp);
    remove("db.txt");
    rename("tmp.txt", "db.txt");
    pthread_mutex_unlock(&file_mutex);
}

void list_records() {
    pthread_mutex_lock(&file_mutex);
    FILE *fp = fopen("db.txt", "rb");
    Record r;
    while (fread(&r, sizeof(Record), 1, fp)) {
        printf("ID: %d, Name: %s\n", r.id, r.name);
    }
    fclose(fp);
    pthread_mutex_unlock(&file_mutex);
}

void* thread_worker(void *arg) {
    while (1) {
        Task task = dequeue();
        printf("[Thread] Processando: %s\n", task.command);

        if (strncmp(task.command, "INSERT", 6) == 0) {
            Record r;
            sscanf(task.command, "INSERT id=%d name='%[^']'", &r.id, r.name);
            insert_record(r);
        } else if (strncmp(task.command, "DELETE", 6) == 0) {
            int id;
            sscanf(task.command, "DELETE id=%d", &id);
            delete_record(id);
        } else if (strncmp(task.command, "LIST", 4) == 0) {
            list_records();
        }
    }
    return NULL;
}

int main() {
    int shmid = shmget(SHM_KEY, sizeof(SharedMemory), IPC_CREAT | 0666);
    shared_data = (SharedMemory*) shmat(shmid, NULL, 0);
    shared_data->ready = 0;

    task_queue.front = 0;
    task_queue.rear = 0;
    task_queue.count = 0;

    pthread_t threads[THREAD_POOL_SIZE];
    for (int i = 0; i < THREAD_POOL_SIZE; i++) {
        pthread_create(&threads[i], NULL, thread_worker, NULL);
    }

    printf("[Servidor] Aguardando comandos do cliente...\n");

    while (1) {
        if (shared_data->ready == 1) {
            if (strcmp(shared_data->message, "EXIT") == 0)
                break;

            Task t;
            strcpy(t.command, shared_data->message);
            enqueue(t);
            shared_data->ready = 0;
        }
        usleep(10000);
    }

    shmdt(shared_data);
    shmctl(shmid, IPC_RMID, NULL);
    return 0;
}
