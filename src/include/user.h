#ifndef TPE_PROTOS_USERS_H
#define TPE_PROTOS_USERS_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include "pop3.h"
#include "stm.h"

#define MAX_USERNAME 40
#define MAX_PASSWORD 40
#define MAX_USERS 1024

struct user {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
};


bool set_maildir();
bool check_password(const char *username, const char *pass);
bool check_login(const char* username, const char* pass);
bool check_user(const char *username);
bool check_user_locked(const char *username, const char *maildir);
bool lock_user(const char *username, const char *maildir);
bool unlock_user(const char *username, const char *maildir);
bool add_user(char* username, char* pass);

#endif