#ifndef SERVER_H
#define SERVER_H

#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>

#define MAX_NAME 64
#define MAX_PAYLOAD 4096

// Global atomic flag for server status
extern atomic_int server_running;

// ===== Command Types =====
typedef enum {
    CMD_UNKNOWN,
    CMD_UPLOAD,
    CMD_LIST,
    CMD_DOWNLOAD,
    CMD_DELETE,
    CMD_PROCESS,
    CMD_LOGIN,
    CMD_SIGNUP
} CommandType;

// ===== Client Queue =====
typedef struct {
    int *buffer;
    int capacity;
    int front, rear, count;
    pthread_mutex_t lock;
    pthread_cond_t not_full;
    pthread_cond_t not_empty;
} ClientQueue;

void initClientQueue(ClientQueue *q, int capacity);
void enqueueClient(ClientQueue *q, int client_fd);
int  dequeueClient(ClientQueue *q);
void destroyClientQueue(ClientQueue *q);

// ===== Task Result =====
typedef struct TaskResult {
    pthread_mutex_t lock;
    pthread_cond_t cond;
    int done;
    char *response;
} TaskResult;

// ===== Task =====
typedef struct Task {
    int cmd;
    char username[64];
    char filename[128];
    char data[4096];
    int data_len;
    TaskResult *result;
} Task;

// ===== Task Node =====
typedef struct TaskNode {
    Task task;
    struct TaskNode *next;
} TaskNode;

// ===== Task Queue =====
typedef struct TaskQueue {
    TaskNode *head;
    TaskNode *tail;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    int count;
    int capacity;
} TaskQueue;

// ===== Task Queue Functions =====
void initTaskQueue(TaskQueue *q, int capacity);
void enqueueTask(TaskQueue *q, Task t);
Task dequeueTask(TaskQueue *q);
void destroyTaskQueue(TaskQueue *q);
void task_queue_destroy(TaskQueue *q);

// ===== Thread Routines =====
void *client_thread_main(void *arg);
void *worker_thread_main(void *arg);

#endif
