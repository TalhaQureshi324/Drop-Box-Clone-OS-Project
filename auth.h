#ifndef AUTH_H
#define AUTH_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

// Simple auth system (for LOGIN/SIGNUP)
void auth_init(void);
void auth_destroy(void);
int auth_signup(const char *username, const char *password);
int auth_login(const char *username, const char *password);

#endif
