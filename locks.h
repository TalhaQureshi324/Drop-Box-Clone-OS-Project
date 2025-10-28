#ifndef LOCKS_H
#define LOCKS_H

#include <pthread.h>

// Structure for per-user locks
typedef struct UserLock {
    char username[64];
    pthread_mutex_t lock;
    struct UserLock *next;
} UserLock;

// Initialization and cleanup
void locks_init(void);
void locks_destroy(void);
void locks_destroy_all(void);   // <--- Make sure this line exists!

// Acquire/release lock for a specific user
void locks_acquire_user(const char *username);
void locks_release_user(const char *username);

// (Optional extension) Acquire/release for a specific file key
void locks_acquire(const char *key);
void locks_release(const char *key);

#endif
