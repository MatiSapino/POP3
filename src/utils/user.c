#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>
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
bool check_user(const char *username, struct Client* Client) {
    for(int i=0; i<userCount; i++){
        if(strcmp(users[i].username, username) == 0){
            okResponse(Client, "user accepted");
            return true;
        }
    }
    errResponse(Client, "invalid user");
    return false;
}

//chequeo de la contraseña
bool check_password(const char *username, const char *pass, struct Client* Client) {
    for(int i=0; i<userCount; i++){
        if(strcmp(users[i].username, username) == 0){
            if(strcmp(users[i].password,pass)==0){
                return true;
            }
        }
    }
    errResponse(Client, "invalid password");
    return false;
}

bool check_login(const char* username, const char* pass, struct Client* Client){
    return check_user(username, Client) && check_password(username, pass, Client);
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
    fprintf(stderr, "\n \n");

    strncpy(users[userCount].username, username,MAX_USERNAME);
    strncpy(users[userCount].password, pass,MAX_PASSWORD);
    userCount++;
    return true;
}

struct mailbox * get_user_mailbox(const char *username) {
    struct mailbox *box = malloc(sizeof(struct mailbox));
    if (box == NULL) {
        return NULL;
    }
    
    memset(box, 0, sizeof(struct mailbox));
    
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s/new", maildir, username);
    
    DIR *dir = opendir(path);
    if (!dir) {
        free(box);
        return NULL;
    }

    struct dirent *entry;
    struct stat st;
    char filepath[PATH_MAX];
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue; // Ignorar archivos ocultos
        
        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);
        
        if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode)) {
            if (box->mail_count >= MAX_MAILS) break;
            
            strncpy(box->mails[box->mail_count].filename, entry->d_name, PATH_MAX-1);
            box->mails[box->mail_count].size = st.st_size;
            box->mails[box->mail_count].deleted = false;
            
            box->total_size += st.st_size;
            box->mail_count++;
        }
    }
    
    closedir(dir);
    return box;
}

