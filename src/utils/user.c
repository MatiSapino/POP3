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
        fprintf(stderr, "DEBUG: No se pudo obtener HOME\n");
        return false;
    }
    
    fprintf(stderr, "DEBUG: HOME path: %s\n", home);
    snprintf(full_path, sizeof(full_path), "%s/maildir", home);
    
    fprintf(stderr, "DEBUG: Intentando usar maildir: %s\n", full_path);
    
    if (mkdir(full_path, 0755) != 0) {
        if (errno == EEXIST) {
            strncpy(maildir, full_path, sizeof(maildir) - 1);
            maildir[sizeof(maildir) - 1] = '\0';
            fprintf(stderr, "DEBUG: Usando maildir existente: %s\n", maildir);
            return true;
        } else {
            fprintf(stderr, "DEBUG: Error creando maildir: %s (errno: %d)\n", full_path, errno);
            return false;
        }
    }
    
    strncpy(maildir, full_path, sizeof(maildir) - 1);
    maildir[sizeof(maildir) - 1] = '\0';
    fprintf(stderr, "DEBUG: Maildir creado en: %s\n", maildir);
    return true;
}
//si existe el usuario
bool check_user(const char *username, struct Client* client) {
    for(int i=0; i<userCount; i++){
        if(strcmp(users[i].username, username) == 0){
            okResponse(client, "user accepted");
            return true;
        }
    }
    errResponse(client, "invalid user");
    return false;
}

//chequeo de la contraseña
bool check_password(const char *username, const char *pass, struct Client* client) {
    for(int i=0; i<userCount; i++){
        if(strcmp(users[i].username, username) == 0){
            if(strcmp(users[i].password,pass)==0){
                return true;
            }
        }
    }
    errResponse(client, "invalid password");
    return false;
}

bool check_login(const char* username, const char* pass, struct Client* client) {
    fprintf(stderr, "DEBUG: Intentando login para usuario: %s\n", username);
    bool user_ok = check_user(username, client);
    bool pass_ok = check_password(username, pass, client);
    fprintf(stderr, "DEBUG: Resultado login - user_ok: %d, pass_ok: %d\n", user_ok, pass_ok);
    return user_ok && pass_ok;
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

    // Create cur, new and tmp directories
    char cur_path[len + 5];
    char new_path[len + 5]; 
    char tmp_path[len + 5];

    snprintf(cur_path, sizeof(cur_path), "%s/cur", path);
    snprintf(new_path, sizeof(new_path), "%s/new", path);
    snprintf(tmp_path, sizeof(tmp_path), "%s/tmp", path);

    if (mkdir(cur_path, 0755) != 0 || 
        mkdir(new_path, 0755) != 0 ||
        mkdir(tmp_path, 0755) != 0) {
        fprintf(stderr, "Couldn't create maildir subdirectories\n");
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

struct mailbox * get_user_mailbox(struct Client* client) {
    return client->mailbox;
}

void free_mailbox(struct mailbox *box){
    free(box);
}

void init_mailbox(struct Client* client){
    struct mailbox *box = malloc(sizeof(struct mailbox));
    if (box == NULL) {
        fprintf(stderr, "Error: No se pudo asignar memoria para mailbox\n");
        return;
    }
    
    memset(box, 0, sizeof(struct mailbox));
    
    char path[PATH_MAX];
    snprintf(path, sizeof(path), "%s/%s/new", maildir, client->user->username);
    fprintf(stderr, "Buscando emails en: %s\n", path);
    
    DIR *dir = opendir(path);
    if (!dir) {
        fprintf(stderr, "Error: No se pudo abrir el directorio %s (errno: %d)\n", path, errno);
        free(box);
        return;
    }

    struct dirent *entry;
    struct stat st;
    char filepath[PATH_MAX];
    
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') {
            fprintf(stderr, "Ignorando archivo oculto: %s\n", entry->d_name);
            continue;
        }
        
        snprintf(filepath, sizeof(filepath), "%s/%s", path, entry->d_name);
        
        if (stat(filepath, &st) == 0 && S_ISREG(st.st_mode)) {
            if (box->mail_count >= MAX_MAILS) {
                fprintf(stderr, "Advertencia: Se alcanzó el límite máximo de emails (%d)\n", MAX_MAILS);
                break;
            }
            
            strncpy(box->mails[box->mail_count].filename, entry->d_name, PATH_MAX-1);
            box->mails[box->mail_count].filename[PATH_MAX-1] = '\0';  // Asegurar terminación null
            box->mails[box->mail_count].size = st.st_size;
            box->mails[box->mail_count].deleted = false;
            
            box->total_size += st.st_size;
            box->mail_count++;
            
            fprintf(stderr, "Email encontrado: %s (tamaño: %lld bytes)\n", entry->d_name, st.st_size);
        }
    }
    
    fprintf(stderr, "Total emails encontrados: %zu, Tamaño total: %zu bytes\n", 
            box->mail_count, box->total_size);
    
    closedir(dir);
    client->mailbox = box;
}

void print_maildir(){
    fprintf(stderr, "%s\n", maildir);
}