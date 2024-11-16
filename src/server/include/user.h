#ifndef TPE_PROTOS_USERS_H
#define TPE_PROTOS_USERS_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include "pop3.h"
#include "stm.h"

#define MAX_USERNAME 40
#define MAX_PASSWORD 40

struct user {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
};


bool user_authenticate(const char *username);
bool check_password(const char *username, const char *pass, const char *maildir);
bool check_user(const char *username, const char *maildir);
bool check_user_locked(const char *username, const char *maildir);
bool lock_user(const char *username, const char *maildir);
bool unlock_user(const char *username, const char *maildir);
struct Client* create_user(int socket, char *buf);

#endif 
