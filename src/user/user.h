#ifndef TPE_PROTOS_USERS_H
#define TPE_PROTOS_USERS_H

#include <stdbool.h>
#include <stdint.h>
#include <unistd.h>
#include "pop3.h"
#include "stm.h"


bool user_authenticate(const char *username);
bool check_password(const char *username, const char *pass, const char *maildir);
bool check_user(const char *username, const char *maildir);
bool check_user_locked(const char *username, const char *maildir);
bool lock_user(const char *username, const char *maildir);
bool unlock_user(const char *username, const char *maildir);
struct Client* create_user(int socket, char *buf);

#endif 
