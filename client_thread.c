
// src/client_thread.c
#define _POSIX_C_SOURCE 200809L
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <stdbool.h>
#include "server.h"
#include "auth.h"
#include <stdatomic.h>

extern ClientQueue g_client_queue;
extern TaskQueue g_task_queue;

static ssize_t read_line(int fd, char *buf, size_t maxlen) {
    size_t idx = 0;
    while (idx < maxlen - 1) {
        char c;
        ssize_t r = read(fd, &c, 1);
        if (r == 0) { // EOF
            break;
        }
        if (r < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        buf[idx++] = c;
        if (c == '\n') break;
    }
    buf[idx] = '\0';
    return (ssize_t)idx;
}

static CommandType parse_command(const char *buf) {
    if (strncmp(buf, "UPLOAD", 6) == 0)   return CMD_UPLOAD;
    if (strncmp(buf, "PROCESS", 7) == 0)  return CMD_PROCESS;
    if (strncmp(buf, "LIST", 4) == 0)     return CMD_LIST;
    if (strncmp(buf, "DOWNLOAD", 8) == 0) return CMD_DOWNLOAD;
    if (strncmp(buf, "DELETE", 6) == 0)   return CMD_DELETE;
    if (strncmp(buf, "LOGIN", 5) == 0)    return CMD_LOGIN;
    if (strncmp(buf, "SIGNUP", 6) == 0)   return CMD_SIGNUP;
    return CMD_UNKNOWN;
}

void *client_thread_main(void *arg) {
    (void)arg;
extern atomic_int server_running;


while (atomic_load(&server_running)) {
    int client_fd = dequeueClient(&g_client_queue);
if (client_fd < 0) {
    /* shutdown requested; exit thread */
    fprintf(stderr, "[Client %lu] shutting down thread\n", (unsigned long)pthread_self());
    break;
}

    if (!atomic_load(&server_running)) break;

        if (client_fd < 0) continue;

        char line[1024];
        char cmdline[1024];
        Task t = {0};
        // First line may be "USER <username>\n" or it may be the command directly.
        ssize_t n = read_line(client_fd, line, sizeof(line));
        if (n <= 0) { close(client_fd); continue; }

        if (strncmp(line, "USER ", 5) == 0) {
            // extract username (without newline)
            sscanf(line + 5, "%63s", t.username);
            // read next line for actual command
            n = read_line(client_fd, cmdline, sizeof(cmdline));
            if (n <= 0) { close(client_fd); continue; }
        } else {
            // no USER header; treat first line as command
            strncpy(cmdline, line, sizeof(cmdline)-1);
            // ensure username is guest by default unless overridden by LOGIN later
            strncpy(t.username, "guest", sizeof(t.username)-1);
        }

        // Remove trailing newline from cmdline for easier parsing
        if (cmdline[strlen(cmdline)-1] == '\n') cmdline[strlen(cmdline)-1] = '\0';

        CommandType cmd = parse_command(cmdline);
        t.cmd = cmd;

        // ---------- SIGNUP ----------
        if (cmd == CMD_SIGNUP) {
            char user[64], pass[64];
            sscanf(cmdline, "SIGNUP %63s %63s", user, pass);
            bool ok = auth_signup(user, pass);
            const char *msg = ok ? "SIGNUP OK\n" : "SIGNUP FAILED (exists)\n";
            write(client_fd, msg, strlen(msg));
            close(client_fd);
            continue;
        }

        // ---------- LOGIN ----------
        if (cmd == CMD_LOGIN) {
            char user[64], pass[64];
            sscanf(cmdline, "LOGIN %63s %63s", user, pass);
            bool ok = auth_login(user, pass);
            const char *msg = ok ? "LOGIN OK\n" : "LOGIN FAILED\n";
            write(client_fd, msg, strlen(msg));
            // Do NOT persist on server side — client will send USER header next time.
            close(client_fd);
            continue;
        }

        // ---------- UPLOAD ----------
        if (cmd == CMD_UPLOAD) {
            char filename[128];
            sscanf(cmdline, "UPLOAD %127s", filename);
            strncpy(t.filename, filename, sizeof(t.filename)-1);

            // read the remainder of socket as file content (until EOF / client close) up to MAX_PAYLOAD
            ssize_t total = 0;
            ssize_t rr;
            while (total < MAX_PAYLOAD - 1) {
                rr = read(client_fd, t.data + total, MAX_PAYLOAD - 1 - total);
                if (rr < 0) {
                    if (errno == EINTR) continue;
                    break;
                }
                if (rr == 0) break; // client closed
                total += rr;
                // keep reading until client closes
            }
            t.data_len = (int)total;
        }

        // ---------- PROCESS ----------
        else if (cmd == CMD_PROCESS) {
            strncpy(t.data, cmdline, sizeof(t.data)-1);
            t.data_len = (int)strlen(t.data);
        }

        // ---------- LIST / DOWNLOAD / DELETE ----------
        else if (cmd == CMD_LIST || cmd == CMD_DOWNLOAD || cmd == CMD_DELETE) {
            strncpy(t.data, cmdline, sizeof(t.data)-1);
            t.data_len = (int)strlen(t.data);
            if (cmd == CMD_DOWNLOAD || cmd == CMD_DELETE) {
                sscanf(cmdline, "%*s %127s", t.filename);
            }
        }

        else {
            write(client_fd, "ERR: Unknown command\n", 20);
            close(client_fd);
            continue;
        }

        // Ensure username default to guest if empty
        if (strlen(t.username) == 0) strncpy(t.username, "guest", sizeof(t.username)-1);

        // Create TaskResult
        TaskResult *res = malloc(sizeof(TaskResult));
        pthread_mutex_init(&res->lock, NULL);
        pthread_cond_init(&res->cond, NULL);
        res->done = 0;
        res->response = NULL;
        t.result = res;

        enqueueTask(&g_task_queue, t);

        // Wait for result
        pthread_mutex_lock(&res->lock);
        while (!res->done) pthread_cond_wait(&res->cond, &res->lock);
        pthread_mutex_unlock(&res->lock);

        // write response (may be binary for DOWNLOAD but server bundles header+data)
        if (res->response) {
            size_t len = strlen(res->response);
            // For DOWNLOAD the response is header+binary; write with write()
            write(client_fd, res->response, len);
        } else {
            write(client_fd, "ERR: No response\n", 17);
        }

fprintf(stderr, "[ClientThread] Exiting...\n");
return NULL;

        // cleanup
        free(res->response);
        pthread_cond_destroy(&res->cond);
        pthread_mutex_destroy(&res->lock);
        free(res);
        close(client_fd);
    }
    return NULL;
}
