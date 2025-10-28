#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include "server.h"

// Use the atomic version declared in server.h
extern atomic_int server_running;

// Initialize client queue
void initClientQueue(ClientQueue *q, int capacity) {
    q->capacity = capacity;
    q->buffer = malloc(sizeof(int) * capacity);
    if (!q->buffer) {
        fprintf(stderr, "Error: malloc failed in initClientQueue\n");
        exit(EXIT_FAILURE);
    }
    q->front = 0;
    q->rear = 0;
    q->count = 0;
    pthread_mutex_init(&q->lock, NULL);
    pthread_cond_init(&q->not_full, NULL);
    pthread_cond_init(&q->not_empty, NULL);
}

// Enqueue a client file descriptor
void enqueueClient(ClientQueue *q, int client_fd) {
    pthread_mutex_lock(&q->lock);

    while (q->count == q->capacity) {
        pthread_cond_wait(&q->not_full, &q->lock);
    }

    q->buffer[q->rear] = client_fd;
    q->rear = (q->rear + 1) % q->capacity;
    q->count++;

    pthread_cond_signal(&q->not_empty);
    pthread_mutex_unlock(&q->lock);
}

// Dequeue a client file descriptor (blocking)
int dequeueClient(ClientQueue *q) {
    pthread_mutex_lock(&q->lock);

    while (q->count == 0 && atomic_load(&server_running)) {
        pthread_cond_wait(&q->not_empty, &q->lock);
    }

    if (q->count == 0 && !atomic_load(&server_running)) {
        // Shutdown in progress — nothing to serve
        pthread_mutex_unlock(&q->lock);
        return -1;
    }

    int fd = q->buffer[q->front];
    q->front = (q->front + 1) % q->capacity;
    q->count--;

    pthread_cond_signal(&q->not_full);
    pthread_mutex_unlock(&q->lock);
    return fd;
}

// Destroy and free queue resources
void destroyClientQueue(ClientQueue *q) {
    if (q->buffer)
        free(q->buffer);

    pthread_mutex_destroy(&q->lock);
    pthread_cond_destroy(&q->not_full);
    pthread_cond_destroy(&q->not_empty);
}
