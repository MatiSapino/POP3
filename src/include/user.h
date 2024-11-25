#ifndef TPE_PROTOS_USERS_H
#define TPE_PROTOS_USERS_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include <limits.h>
#include "pop3.h"
#include "stm.h"

#define MAX_USERNAME 40
#define MAX_PASSWORD 40
#define MAX_USERS 1024
#define MAX_MAILS 1000

struct user {
    char username[MAX_USERNAME];
    char password[MAX_PASSWORD];
};

struct mail {
    char filename[PATH_MAX];
    size_t size;
    bool deleted;
};

struct mailbox {
    struct mail mails[MAX_MAILS];
    size_t mail_count;
    size_t total_size;
};

struct Client;

bool set_maildir();
bool check_password(const char *username, const char *pass, struct Client* client);
bool check_login(const char* username, const char* pass, struct Client* client);
bool check_user(const char *username, struct Client* client);
bool check_user_locked(const char *username, const char *maildir);
bool lock_user(const char *username, const char *maildir);
bool unlock_user(const char *username, const char *maildir);
bool add_user(char* username, char* pass);
struct mailbox * get_user_mailbox(const char *username);

#endif