// client/client.c
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 9000
#define MAX_BUF 4096
#define SESSION_FILE ".session_user"

// --- Helper functions ---

static ssize_t robust_read(int fd, void *buf, size_t count) {
    ssize_t r;
    do {
        r = read(fd, buf, count);
    } while (r < 0 && errno == EINTR);
    return r;
}

static ssize_t robust_write(int fd, const void *buf, size_t count) {
    const char *p = buf;
    size_t sent = 0;
    while (sent < count) {
        ssize_t w = write(fd, p + sent, count - sent);
        if (w < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        sent += (size_t)w;
    }
    return (ssize_t)sent;
}

/* Read saved session username (if any). */
static void read_session(char *out, size_t n) {
    if (n == 0) return;
    out[0] = '\0';
    FILE *f = fopen(SESSION_FILE, "r");
    if (!f) return;
    if (fgets(out, (int)n, f) != NULL) {
        size_t L = strlen(out);
        if (L > 0 && out[L - 1] == '\n') out[L - 1] = '\0';
    }
    fclose(f);
}

static void write_session(const char *user) {
    FILE *f = fopen(SESSION_FILE, "w");
    if (!f) return;
    fprintf(f, "%s", user);
    fclose(f);
}

static void clear_session(void) {
    unlink(SESSION_FILE);
}

// --- Main client program ---

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage:\n");
        printf("  %s SIGNUP <username> <password>\n", argv[0]);
        printf("  %s LOGIN <username> <password>\n", argv[0]);
        printf("  %s LOGOUT\n", argv[0]);
        printf("  %s UPLOAD <file>\n", argv[0]);
        printf("  %s LIST\n", argv[0]);
        printf("  %s DOWNLOAD <file>\n", argv[0]);
        printf("  %s DELETE <file>\n", argv[0]);
        printf("  %s PROCESS <seconds>\n", argv[0]);
        return 1;
    }

    // Load persisted session (if any)
    char session_user[64] = "";
    read_session(session_user, sizeof(session_user));

    // Handle local logout
    if (strcmp(argv[1], "LOGOUT") == 0) {
        clear_session();
        printf("Logged out (local session cleared).\n");
        return 0;
    }

    // Build command line
    char cmdline[1024] = {0};
    if (strcmp(argv[1], "SIGNUP") == 0 && argc == 4) {
        snprintf(cmdline, sizeof(cmdline), "SIGNUP %s %s\n", argv[2], argv[3]);
    } else if (strcmp(argv[1], "LOGIN") == 0 && argc == 4) {
        snprintf(cmdline, sizeof(cmdline), "LOGIN %s %s\n", argv[2], argv[3]);
    } else if (strcmp(argv[1], "UPLOAD") == 0 && argc == 3) {
        snprintf(cmdline, sizeof(cmdline), "UPLOAD %s\n", argv[2]);
    } else if (strcmp(argv[1], "LIST") == 0) {
        snprintf(cmdline, sizeof(cmdline), "LIST\n");
    } else if (strcmp(argv[1], "DOWNLOAD") == 0 && argc == 3) {
        snprintf(cmdline, sizeof(cmdline), "DOWNLOAD %s\n", argv[2]);
    } else if (strcmp(argv[1], "DELETE") == 0 && argc == 3) {
        snprintf(cmdline, sizeof(cmdline), "DELETE %s\n", argv[2]);
    } else if (strcmp(argv[1], "PROCESS") == 0 && argc == 3) {
        snprintf(cmdline, sizeof(cmdline), "PROCESS %s\n", argv[2]);
    } else {
        printf("Invalid command or wrong arguments.\n");
        return 1;
    }

    // Create socket
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) { perror("socket"); return 1; }

    struct sockaddr_in serv = {0};
    serv.sin_family = AF_INET;
    serv.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serv.sin_addr);

    if (connect(sock, (struct sockaddr *)&serv, sizeof(serv)) < 0) {
        perror("connect");
        close(sock);
        return 1;
    }

    // Send user header (if logged in and not an auth command)
    int is_auth_cmd = (strncmp(cmdline, "LOGIN", 5) == 0 || strncmp(cmdline, "SIGNUP", 6) == 0);
    if (!is_auth_cmd && session_user[0] != '\0') {
        char hdr[128];
        snprintf(hdr, sizeof(hdr), "USER %s\n", session_user);
        if (robust_write(sock, hdr, strlen(hdr)) < 0) {
            perror("write");
            close(sock);
            return 1;
        }
    }

    // --- Authentication commands ---
    if (strncmp(cmdline, "SIGNUP", 6) == 0 || strncmp(cmdline, "LOGIN", 5) == 0) {
        if (robust_write(sock, cmdline, strlen(cmdline)) < 0) {
            perror("write"); close(sock); return 1;
        }

        char resp[MAX_BUF];
        ssize_t r = robust_read(sock, resp, sizeof(resp) - 1);
        if (r > 0) {
            resp[r] = '\0';
            printf("Server response:\n%s\n", resp);

            if (strncmp(cmdline, "LOGIN", 5) == 0 && strstr(resp, "LOGIN OK")) {
                write_session(argv[2]);
                strncpy(session_user, argv[2], sizeof(session_user) - 1);
            }
        } else {
            printf("No response from server.\n");
        }
        close(sock);
        return 0;
    }

    // --- UPLOAD ---
    if (strncmp(cmdline, "UPLOAD", 6) == 0) {
        const char *filename = argv[2];
        FILE *f = fopen(filename, "rb");
        if (!f) { perror("fopen"); close(sock); return 1; }

        if (robust_write(sock, cmdline, strlen(cmdline)) < 0) {
            perror("write"); fclose(f); close(sock); return 1;
        }

        char filebuf[4096];
        size_t n;
        while ((n = fread(filebuf, 1, sizeof(filebuf), f)) > 0) {
            if (robust_write(sock, filebuf, n) < 0) {
                perror("write"); fclose(f); close(sock); return 1;
            }
        }
        fclose(f);
        shutdown(sock, SHUT_WR);

        char resp[MAX_BUF];
        ssize_t r = robust_read(sock, resp, sizeof(resp) - 1);
        if (r > 0) {
            resp[r] = '\0';
            printf("Server response:\n%s\n", resp);
        } else {
            printf("No response after upload.\n");
        }
        close(sock);
        return 0;
    }

    // --- DOWNLOAD ---
    if (strncmp(cmdline, "DOWNLOAD", 8) == 0) {
        if (robust_write(sock, cmdline, strlen(cmdline)) < 0) { perror("write"); close(sock); return 1; }

        char header[64] = {0};
        size_t idx = 0;
        char c;
        while (idx < sizeof(header) - 1) {
            ssize_t rr = robust_read(sock, &c, 1);
            if (rr <= 0) break;
            header[idx++] = c;
            if (c == '\n') break;
        }
        header[idx] = '\0';

        if (strncmp(header, "SIZE", 4) != 0) {
            char rest[MAX_BUF];
            ssize_t rr = robust_read(sock, rest, sizeof(rest) - 1);
            if (rr > 0) {
                rest[rr] = '\0';
                printf("Server response:\n%s%s\n", header, rest);
            } else {
                printf("Server returned: %s\n", header);
            }
            close(sock);
            return 0;
        }

        size_t filesize = 0;
        sscanf(header, "SIZE %zu", &filesize);
        if (filesize == 0) {
            printf("Server reported empty file.\n");
            close(sock);
            return 0;
        }

        char *buffile = malloc(filesize);
        if (!buffile) { fprintf(stderr, "malloc failed\n"); close(sock); return 1; }

        size_t received = 0;
        while (received < filesize) {
            ssize_t rr = robust_read(sock, buffile + received, filesize - received);
            if (rr <= 0) break;
            received += (size_t)rr;
        }

        char outname[512];
        snprintf(outname, sizeof(outname), "downloaded_%s", argv[2]);
        FILE *of = fopen(outname, "wb");
        if (!of) { perror("fopen"); free(buffile); close(sock); return 1; }
        fwrite(buffile, 1, received, of);
        fclose(of);
        free(buffile);

        printf("Downloaded %zu bytes → saved as %s\n", received, outname);
        close(sock);
        return 0;
    }

    // --- PROCESS ---
    if (strncmp(cmdline, "PROCESS", 7) == 0) {
        if (robust_write(sock, cmdline, strlen(cmdline)) < 0) {
            perror("write"); close(sock); return 1;
        }

        char resp[MAX_BUF];
        ssize_t r = robust_read(sock, resp, sizeof(resp) - 1);
        if (r > 0) {
            resp[r] = '\0';
            printf("Server response:\n%s\n", resp);
        } else {
            printf("No response from server.\n");
        }
        close(sock);
        return 0;
    }

    // --- LIST / DELETE / others ---
    if (robust_write(sock, cmdline, strlen(cmdline)) < 0) { perror("write"); close(sock); return 1; }

    char resp[MAX_BUF];
    ssize_t r = robust_read(sock, resp, sizeof(resp) - 1);
    if (r > 0) {
        resp[r] = '\0';
        printf("Server response:\n%s\n", resp);
    } else {
        printf("No response.\n");
    }

    close(sock);
    return 0;
}
