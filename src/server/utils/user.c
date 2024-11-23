#include <string.h>

#include "user.h"
#include "pop3.h"

static size_t users_count = 0;

static struct user users[MAX_USERS];
static int userCount = 0;

//validacion del nombre de usuario seguro
bool user_authenticate(const char *username) {
    if (!*username || *username =='.' ){
        return false;
    }
    for (const char *n = username; *n; n++) {
        if (*n == '/' || (n - username) > MAX_USERNAME_LENGTH) {
            return false;
        }
    }
    return true;
}

//si existe el usuario
bool check_user(const char *username) {
    for(int i=0; i<userCount; i++){
        if(strcmp(users[i].username, username) == 0){
            return true;
        }
    }
    return false;
}

//chequeo de la contraseña
bool check_password(const char *username, const char *pass) {
    for(int i=0; i<userCount; i++){
        if(strcmp(users[i].username, username) == 0){
            if(strcmp(users[i].password,pass)==0){
                return true;
            }
        }
    }
    return false;
}

bool check_login(const char* username, const char* pass){
    return check_user(username) && check_password(username, pass);
}


bool check_user_locked(const char *username, const char *maildir){
    char path[strlen(maildir) + MAX_USERNAME_LENGTH + sizeof("/lock")];
    snprintf(path, sizeof(path), "%s/%s/lock", maildir, username);
    return access(path, F_OK) != -1;
}

bool lock_user(const char *username, const char *maildir){
    char path[strlen(maildir) + MAX_USERNAME_LENGTH + sizeof("/lock")];
    snprintf(path, sizeof(path), "%s/%s/lock", maildir, username);
    FILE *file = fopen(path, "w");
    if (!file) return false;
    fclose(file);
    return true;
}

bool unlock_user(const char *username, const char *maildir){
    char path[strlen(maildir) + MAX_USERNAME_LENGTH + sizeof("/lock")];
    snprintf(path, sizeof(path), "%s/%s/lock", maildir, username);
    return remove(path) == 0;
}