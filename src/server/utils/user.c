#include <string.h>

#include "user.h"

static size_t users_count = 0;


// struct Client * create_user(int socket, char *buf){
//     struct Client * new_client = malloc(sizeof(Client));
//     struct state_machine* sm = malloc(sizeof(struct state_machine));
//     sm->initial = STATE_WAIT_USER;
//     new_client->socket_fd = socket;
//     new_client->stm = sm;
//     // new_client->buffer = buf; 
//     return new_client;
// }

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
bool check_user(const char *username, const char *maildir) {
    char path[strlen(maildir) + MAX_USERNAME_LENGTH + 1];
    snprintf(path, sizeof(path), "%s/%s", maildir, username); //imprime el path del user 
    return access(path, R_OK) != -1; //chequeo si ese archivo existe, si existe --> existe el user
}

//chequeo de la contraseña
bool check_password(const char *username, const char *pass, const char *maildir) {
    char path[strlen(maildir) + MAX_USERNAME_LENGTH + sizeof("/data/pass")];
    snprintf(path, sizeof(path), "%s/%s/data/pass", maildir, username);
    FILE *file = fopen(path, "r");
    if (!file) return false;
    char buffer[MAX_PASSWORD_LENGTH + 1];
    fgets(buffer, sizeof(buffer), file);
    fclose(file);
    return strcmp(buffer, pass) == 0;
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