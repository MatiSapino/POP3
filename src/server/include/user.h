#ifndef TPE_PROTOS_USERS_H
#define TPE_PROTOS_USERS_H

#include <stdint.h>
#include <unistd.h>

struct user {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
};

int user_authenticate(char *username, char *password);
bool check_password(const char *username, const char *pass, const char *maildir);
bool check_user(const char *username, const char *maildir);
bool check_user_locked(const char *username, const char *maildir);
bool lock_user(const char *username, const char *maildir);
bool unlock_user(const char *username, const char *maildir);

#endif 
