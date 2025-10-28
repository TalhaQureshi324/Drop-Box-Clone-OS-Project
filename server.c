#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <pthread.h>

#include "server.h"
#include "locks.h"
#include "auth.h"

#define PORT 9000
#define MAX_CLIENTS 10
#define MAX_WORKERS 4

// ========== GLOBAL VARIABLES ==========
atomic_int server_running = 1;
int listen_fd = -1;

ClientQueue g_client_queue;
TaskQueue g_task_queue;

pthread_t client_threads[MAX_CLIENTS];
pthread_t worker_threads[MAX_WORKERS];

// ========== FUNCTION DECLARATIONS ==========
void wake_all_threads(void);
void shutdown_server(pthread_t *client_threads, pthread_t *worker_threads);
void handle_sigint(int sig);

// ========== SIGNAL HANDLER ==========
void handle_sigint(int sig) {
    (void)sig;
    const char msg[] = "\n[Server] Caught SIGINT — shutting down gracefully...\n";
    write(STDERR_FILENO, msg, sizeof(msg) - 1);
    atomic_store(&server_running, 0);

    // Close listening socket (async-signal-safe)
    if (listen_fd >= 0) {
        close(listen_fd);
        listen_fd = -1;
    }
}

// ========== WAKE THREADS ==========
void wake_all_threads(void) {
    // Wake waiting client threads (if any)
    pthread_mutex_lock(&g_client_queue.lock);
    pthread_cond_broadcast(&g_client_queue.not_empty);
    pthread_cond_broadcast(&g_client_queue.not_full);
    pthread_mutex_unlock(&g_client_queue.lock);

    // Wake waiting worker threads (if any)
    pthread_mutex_lock(&g_task_queue.mutex);
    pthread_cond_broadcast(&g_task_queue.cond);
    pthread_mutex_unlock(&g_task_queue.mutex);
}

// ========== SERVER MAIN ==========
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    struct sockaddr_in addr;
    int opt = 1;

    signal(SIGINT, handle_sigint);
    printf("Starting server initialization...\n");

    // Initialize subsystems
    initClientQueue(&g_client_queue, MAX_CLIENTS);
    initTaskQueue(&g_task_queue, 100);
    locks_init();
    auth_init();

    // Create socket
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(listen_fd, 10) < 0) {
        perror("listen");
        close(listen_fd);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d...\n", PORT);

    // Spawn worker threads
    for (int i = 0; i < MAX_WORKERS; i++) {
        pthread_create(&worker_threads[i], NULL, worker_thread_main, NULL);
    }

    // Spawn client threads
    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_create(&client_threads[i], NULL, client_thread_main, NULL);
    }

    // Main accept loop
    while (atomic_load(&server_running)) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &client_len);

        if (!atomic_load(&server_running))
            break;

        if (client_fd < 0) {
            if (atomic_load(&server_running))
                perror("accept");
            continue;
        }

        printf("Accepted new client connection.\n");
        enqueueClient(&g_client_queue, client_fd);
    }

    // If accept() unblocked due to SIGINT or error
    wake_all_threads();
    shutdown_server(client_threads, worker_threads);
    return 0;
}

// ========== SHUTDOWN SERVER ==========
void shutdown_server(pthread_t *client_threads, pthread_t *worker_threads) {
    fprintf(stderr, "[Server] Initiating shutdown sequence...\n");

    // Step 1: Stop accepting new clients
    atomic_store(&server_running, 0);
    wake_all_threads();

    // Step 2: Enqueue sentinel EXIT tasks for workers
    Task sentinel = {0};
    sentinel.cmd = CMD_UNKNOWN;
    strncpy(sentinel.data, "EXIT_WORKER", sizeof(sentinel.data) - 1);
    sentinel.result = NULL;

    for (int i = 0; i < MAX_WORKERS; i++) {
        enqueueTask(&g_task_queue, sentinel);
    }

    // Step 3: Wait for threads to exit
    fprintf(stderr, "[Server] Waiting for threads to finish...\n");
    for (int i = 0; i < MAX_CLIENTS; i++) {
        pthread_join(client_threads[i], NULL);
    }
    for (int i = 0; i < MAX_WORKERS; i++) {
        pthread_join(worker_threads[i], NULL);
    }

    // Step 4: Destroy all queues & locks safely
    destroyClientQueue(&g_client_queue);
    destroyTaskQueue(&g_task_queue);
    locks_destroy_all();
    auth_destroy();

    fprintf(stderr, "[Server] Shutdown complete.\n");
}
