#include "auth.h"
#include <sys/stat.h>

static pthread_mutex_t auth_lock = PTHREAD_MUTEX_INITIALIZER;
static const char *USER_FILE = "users.txt";

void auth_init(void) {
    FILE *f = fopen(USER_FILE, "a");
    if (f) fclose(f);
}

void auth_destroy(void) {
    pthread_mutex_destroy(&auth_lock);
}

int auth_signup(const char *username, const char *password) {
    pthread_mutex_lock(&auth_lock);
    FILE *f = fopen(USER_FILE, "r");
    char u[128], p[128];
    if (f) {
        while (fscanf(f, "%127s %127s", u, p) == 2) {
            if (strcmp(u, username) == 0) {
                fclose(f);
                pthread_mutex_unlock(&auth_lock);
                return 0; // user exists
            }
        }
        fclose(f);
    }

    f = fopen(USER_FILE, "a");
    if (!f) {
        pthread_mutex_unlock(&auth_lock);
        return 0;
    }
    fprintf(f, "%s %s\n", username, password);
    fclose(f);
    pthread_mutex_unlock(&auth_lock);
    return 1;
}

int auth_login(const char *username, const char *password) {
    pthread_mutex_lock(&auth_lock);
    FILE *f = fopen(USER_FILE, "r");
    char u[128], p[128];
    if (!f) {
        pthread_mutex_unlock(&auth_lock);
        return 0;
    }
    int ok = 0;
    while (fscanf(f, "%127s %127s", u, p) == 2) {
        if (strcmp(u, username) == 0 && strcmp(p, password) == 0) {
            ok = 1;
            break;
        }
    }
    fclose(f);
    pthread_mutex_unlock(&auth_lock);
    return ok;
}
