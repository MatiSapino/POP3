#include "pop3.h"

static const char* maildir;
static Connection connections[MAX_CLIENTS] = {0}; 

// Setea la ruta del directorio de mails, usando un valor por defecto si no se pasa un argumento.
static void set_maildir(const char *dir) {
    maildir = dir ? dir : "./dist/mail";
}

// Crea el directorio de mails si no existe.
static void create_maildir_if_needed() {
    if (access(maildir, F_OK) == -1) {
        mkdir(maildir, S_IRWXU);
    }
}

static  void reset_connections() {
    for (size_t i = 0; i < MAX_CLIENTS; i++) {
        connections[i].username[0] = 0;
        connections[i].authenticated = false;
    }
}

void pop_init(char *dir) {
    set_maildir(dir);
    create_maildir_if_needed();
    reset_connections();
}

