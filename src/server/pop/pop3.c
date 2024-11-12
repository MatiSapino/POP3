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

int initialize_pop_connection(int sock_fd, struct sockaddr_in client_address)
{
    Connection *client_connection = &connections[sock_fd];
    client_connection->username[0] = '\0';
    client_connection->authenticated = false;

    const char welcome_msg[] = OK_RESPONSE(" POP3 server ready");
    ssize_t sent_bytes = send(sock_fd, welcome_msg, strlen(welcome_msg), 0);
    
    if (sent_bytes < 0)
    {
        LOG("Error sending welcome message to client\n");
        return -1; // Error
    }
    
    return 0; // Success
}
