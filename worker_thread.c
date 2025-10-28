#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "server.h"
#include "locks.h"

extern TaskQueue g_task_queue;

/* ---------- Helper Functions ---------- */

void save_file(const char *path, const char *data, int len) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        perror("fopen");
        return;
    }
    fwrite(data, 1, len, f);
    fclose(f);
}

static void make_userdir_if_needed(const char *user) {
    mkdir("storage", 0755);
    char userdir[256];
    snprintf(userdir, sizeof(userdir), "storage/%s", user && strlen(user) ? user : "guest");
    mkdir(userdir, 0755);
}

/* helper to produce file key: "user/filename" */
static void make_file_key(char *out, size_t outlen, const char *user, const char *filename) {
    snprintf(out, outlen, "%s/%s", user && strlen(user) ? user : "guest", filename ? filename : "");
}

/* ---------- Worker Thread Main ---------- */

void *worker_thread_main(void *arg) {
    (void)arg;

    locks_init();

    while (1) {
        Task t = dequeueTask(&g_task_queue);
/* If sentinel for shutdown (no result pointer and special data), exit thread */
if (t.result == NULL && strncmp(t.data, "EXIT_WORKER", 11) == 0) {
    fprintf(stderr, "[Worker %lu] received EXIT_WORKER sentinel — exiting\n",
            (unsigned long)pthread_self());
    break;
}
        /* Check for sentinel task (server shutdown) */
        if (strncmp(t.data, "EXIT_WORKER", 11) == 0) {
            fprintf(stderr, "[WorkerThread] Received shutdown signal. Exiting...\n");
            break;
        }

        const char *user = (strlen(t.username) > 0) ? t.username : "guest";

        // ===== UPLOAD =====
        if (t.cmd == CMD_UPLOAD) {
            locks_acquire_user(user);

            mkdir("storage", 0755);
            char userdir[128];
            snprintf(userdir, sizeof(userdir), "storage/%s", user);
            mkdir(userdir, 0755);

            char path[256];
            snprintf(path, sizeof(path), "%s/%s", userdir, t.filename);
            save_file(path, t.data, t.data_len);

            pthread_mutex_lock(&t.result->lock);
            t.result->response = strdup("UPLOAD OK\n");
            t.result->done = 1;
            pthread_cond_signal(&t.result->cond);
            pthread_mutex_unlock(&t.result->lock);

            locks_release_user(user);
        }

        // ===== LIST =====
        else if (t.cmd == CMD_LIST) {
            locks_acquire_user(user);
            make_userdir_if_needed(user);

            char userdir[256];
            snprintf(userdir, sizeof(userdir), "storage/%s", user);

            char cmd[512];
            snprintf(cmd, sizeof(cmd), "ls %s 2>/dev/null", userdir);

            FILE *p = popen(cmd, "r");
            char buf[1024] = {0};
            if (p) {
                fread(buf, 1, sizeof(buf)-1, p);
                pclose(p);
            }
            locks_release_user(user);

            pthread_mutex_lock(&t.result->lock);
            t.result->response = (strlen(buf) == 0)
                ? strdup("No files found\n")
                : strdup(buf);
            t.result->done = 1;
            pthread_cond_signal(&t.result->cond);
            pthread_mutex_unlock(&t.result->lock);
        }

        // ===== DOWNLOAD =====
        else if (t.cmd == CMD_DOWNLOAD) {
            char filekey[512];
            make_file_key(filekey, sizeof(filekey), user, t.filename);
            locks_acquire(filekey);

            char path[512];
            snprintf(path, sizeof(path), "storage/%s/%s", user, t.filename);

            FILE *f = fopen(path, "rb");
            if (!f) {
                locks_release(filekey);
                pthread_mutex_lock(&t.result->lock);
                t.result->response = strdup("ERR: File not found\n");
                t.result->done = 1;
                pthread_cond_signal(&t.result->cond);
                pthread_mutex_unlock(&t.result->lock);
                continue;
            }

            fseek(f, 0, SEEK_END);
            long filesize = ftell(f);
            rewind(f);

            char *data = malloc(filesize ? filesize : 1);
            size_t n = fread(data, 1, filesize, f);
            fclose(f);

            char header[64];
            snprintf(header, sizeof(header), "SIZE %zu\n", n);

            size_t total_len = strlen(header) + n;
            char *resp = malloc(total_len + 1);
            memcpy(resp, header, strlen(header));
            memcpy(resp + strlen(header), data, n);
            resp[total_len] = '\0';
            free(data);

            locks_release(filekey);

            pthread_mutex_lock(&t.result->lock);
            t.result->response = resp;
            t.result->done = 1;
            pthread_cond_signal(&t.result->cond);
            pthread_mutex_unlock(&t.result->lock);
        }

        // ===== DELETE =====
        else if (t.cmd == CMD_DELETE) {
            char filekey[512];
            make_file_key(filekey, sizeof(filekey), user, t.filename);
            locks_acquire(filekey);

            char path[512];
            snprintf(path, sizeof(path), "storage/%s/%s", user, t.filename);
            int res = unlink(path);

            locks_release(filekey);

            pthread_mutex_lock(&t.result->lock);
            t.result->response = (res == 0)
                ? strdup("DELETE OK\n")
                : strdup("ERR: Delete failed\n");
            t.result->done = 1;
            pthread_cond_signal(&t.result->cond);
            pthread_mutex_unlock(&t.result->lock);
        }

        // ===== PROCESS =====
        else if (t.cmd == CMD_PROCESS) {
            int secs = 1;
            sscanf(t.data, "PROCESS %d", &secs);
            fprintf(stderr, "Worker %ld processing %d seconds...\n",
                    pthread_self(), secs);
            sleep(secs);

            pthread_mutex_lock(&t.result->lock);
            t.result->response = strdup("DONE PROCESS\n");
            t.result->done = 1;
            pthread_cond_signal(&t.result->cond);
            pthread_mutex_unlock(&t.result->lock);
        }

        // ===== UNKNOWN =====
        else {
            pthread_mutex_lock(&t.result->lock);
            t.result->response = strdup("ERR: Unknown command\n");
            t.result->done = 1;
            pthread_cond_signal(&t.result->cond);
            pthread_mutex_unlock(&t.result->lock);
        }
    }

    fprintf(stderr, "[WorkerThread] Exiting cleanly.\n");
    return NULL;
}

