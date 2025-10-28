#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <stdatomic.h>
#include "server.h"

// Use atomic version of server_running (from server.h)
extern atomic_int server_running;

// Initialize the task queue
void initTaskQueue(TaskQueue *q, int capacity) {
    q->head = q->tail = NULL;
    q->count = 0;
    q->capacity = capacity;
    pthread_mutex_init(&q->mutex, NULL);
    pthread_cond_init(&q->cond, NULL);
}

// Enqueue a new task into the queue
void enqueueTask(TaskQueue *q, Task t) {
    pthread_mutex_lock(&q->mutex);

    // Create new node
    TaskNode *node = malloc(sizeof(TaskNode));
    if (!node) {
        pthread_mutex_unlock(&q->mutex);
        fprintf(stderr, "Error: malloc failed in enqueueTask\n");
        return;
    }

    node->task = t;
    node->next = NULL;

    // Add to tail
    if (q->tail)
        q->tail->next = node;
    else
        q->head = node;
    q->tail = node;

    q->count++;
    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
}

// Dequeue a task from the queue (blocking)
Task dequeueTask(TaskQueue *q) {
    pthread_mutex_lock(&q->mutex);

    // Wait until there is a task or server is shutting down
    while (q->count == 0 && atomic_load(&server_running)) {
        pthread_cond_wait(&q->cond, &q->mutex);
    }

    Task t;
    memset(&t, 0, sizeof(t));

    // If server is shutting down and no tasks left
    if (q->count == 0 && !atomic_load(&server_running)) {
        t.cmd = CMD_UNKNOWN;
        strncpy(t.data, "EXIT_WORKER", sizeof(t.data) - 1);
        t.result = NULL;
        pthread_mutex_unlock(&q->mutex);
        return t;
    }

    // Normal dequeue
    TaskNode *node = q->head;
    if (!node) {
        pthread_mutex_unlock(&q->mutex);
        t.cmd = CMD_UNKNOWN;
        t.result = NULL;
        return t;
    }

    t = node->task;
    q->head = node->next;
    if (!q->head)
        q->tail = NULL;

    free(node);
    q->count--;

    pthread_cond_signal(&q->cond);
    pthread_mutex_unlock(&q->mutex);
    return t;
}

// Destroy queue and free memory
void destroyTaskQueue(TaskQueue *q) {
    pthread_mutex_lock(&q->mutex);

    TaskNode *cur = q->head;
    while (cur) {
        TaskNode *next = cur->next;
        free(cur);
        cur = next;
    }

    q->head = q->tail = NULL;
    q->count = 0;

    pthread_mutex_unlock(&q->mutex);
    pthread_mutex_destroy(&q->mutex);
    pthread_cond_destroy(&q->cond);
}

// Cleanup for server shutdown
void task_queue_destroy(TaskQueue *q) {
    destroyTaskQueue(q);
}
