#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include "user.h"
#include "pop3.h"

static size_t users_count = 0;

static struct user users[MAX_USERS];
static int userCount = 0;
static char maildir[100];

bool set_maildir() {
    char full_path[256]; 

    char* home = getenv("HOME");
    if (home == NULL) {
        return false;
    }
    snprintf(full_path, sizeof(full_path), "%s/maildir", home);
    
    if (mkdir(full_path, 0755) != 0) {
        if (errno == EEXIST) {
            strncpy(maildir, full_path, sizeof(maildir) - 1);
            maildir[sizeof(maildir) - 1] = '\0';
            return true;
        } else {
            return false;
        }
    }
    strncpy(maildir, full_path, sizeof(maildir) - 1);
    maildir[sizeof(maildir) - 1] = '\0';
    fprintf(stderr, "Maildir created at %s\n", maildir);
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


bool add_user(char* username, char* pass){
    fprintf(stderr, "Adding user %s\n", username);
    if(userCount>=MAX_USERS){
        return false;
    }
    size_t len = strlen(maildir) + strlen(username) + 2;

    char path[len]; 
    snprintf(path, sizeof(path), "%s/%s", maildir, username);
    fprintf(stderr, "Creating user dir at: %s\n", path);
    if (mkdir(path, 0755) != 0) {
        fprintf(stderr, "Couldn't create user dir\n");
        return false;
    }

    for(int i = 0; i < userCount;i++){
        if(strcmp(users[i].username, username)==0){
            return false; 
        }
    }
    strncpy(users[userCount].username, username,MAX_USERNAME);
    strncpy(users[userCount].password, pass,MAX_PASSWORD);
    userCount++;
    return true;
}