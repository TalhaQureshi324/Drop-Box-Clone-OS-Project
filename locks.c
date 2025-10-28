#include "locks.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static UserLock *lock_table = NULL;
static pthread_mutex_t table_mutex = PTHREAD_MUTEX_INITIALIZER;

// Initialize global lock table
void locks_init(void) {
    pthread_mutex_lock(&table_mutex);
    lock_table = NULL;
    pthread_mutex_unlock(&table_mutex);
}

// Find or create lock for username
static pthread_mutex_t *get_user_lock(const char *username) {
    pthread_mutex_lock(&table_mutex);

    UserLock *cur = lock_table;
    while (cur) {
        if (strcmp(cur->username, username) == 0) {
            pthread_mutex_unlock(&table_mutex);
            return &cur->lock;
        }
        cur = cur->next;
    }

    // Not found → create one
    UserLock *new_lock = malloc(sizeof(UserLock));
    if (!new_lock) {
        pthread_mutex_unlock(&table_mutex);
        fprintf(stderr, "Memory alloc failed in get_user_lock\n");
        return NULL;
    }
    strncpy(new_lock->username, username, sizeof(new_lock->username) - 1);
    new_lock->username[sizeof(new_lock->username) - 1] = '\0';
    pthread_mutex_init(&new_lock->lock, NULL);
    new_lock->next = lock_table;
    lock_table = new_lock;

    pthread_mutex_unlock(&table_mutex);
    return &new_lock->lock;
}

void locks_acquire_user(const char *username) {
    pthread_mutex_t *lock = get_user_lock(username);
    if (lock) pthread_mutex_lock(lock);
}

void locks_release_user(const char *username) {
    pthread_mutex_t *lock = get_user_lock(username);
    if (lock) pthread_mutex_unlock(lock);
}

// These are placeholders for optional file-based locks (can be same as user locks)
void locks_acquire(const char *key) {
    locks_acquire_user(key);
}

void locks_release(const char *key) {
    locks_release_user(key);
}

void locks_destroy(void) {
    pthread_mutex_lock(&table_mutex);
    UserLock *cur = lock_table;
    while (cur) {
        UserLock *next = cur->next;
        pthread_mutex_destroy(&cur->lock);
        free(cur);
        cur = next;
    }
    lock_table = NULL;
    pthread_mutex_unlock(&table_mutex);
}
void locks_destroy_all(void) {
    pthread_mutex_lock(&table_mutex);
    UserLock *cur = lock_table;
    while (cur) {
        UserLock *next = cur->next;
        pthread_mutex_destroy(&cur->lock);
        free(cur);
        cur = next;
    }
    lock_table = NULL;
    pthread_mutex_unlock(&table_mutex);
    pthread_mutex_destroy(&table_mutex);
}
