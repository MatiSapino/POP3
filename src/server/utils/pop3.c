#include "pop3.h"

//struct command_function {
//    char *name;
//    enum pop3_state (*function)(struct selector_key *key, struct command *command);
//};
///*

//
//
static const char* maildir;
static Client connections[MAX_CLIENTS] = {0};
#define OK_RESPONSE(m) OK_RESP " " m ENTER
#define ERR_RESPONSE(m) ERR_RESP " " m ENTER
//
//
//// Función para generar una respuesta OK
const char* ok_respon(const char *message) {
   static char response[MAX_POP3_RESPONSE_LENGTH];
   snprintf(response, sizeof(response), "%s %s%s", OK_RESP, message, ENTER);
   return response;
}

//// Función para generar una respuesta de error
const char* err_respon(const char *message) {
   static char response[MAX_POP3_RESPONSE_LENGTH];
   snprintf(response, sizeof(response), "%s %s%s", ERR_RESP, message, ENTER);
   return response;
}


// Setea la ruta del directorio de mails, usando un valor por defecto si no se pasa un argumento.
static void set_maildir(const char *dir) {
   maildir = dir ? dir : "./dist/mail";
}

//// Crea el directorio de mails si no existe.
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
   Client *client_connection = &connections[sock_fd];
   client_connection->username[0] = '\0';
   client_connection->authenticated = false;

   const char welcome_msg[] = OK_RESPONSE(" POP3 server ready");
   ssize_t sent_bytes = send(sock_fd, welcome_msg, strlen(welcome_msg), 0);

   if (sent_bytes < 0)
   {
       fprintf(stderr, "Error sending welcome message to user\n");
       return -1; // Error
   }

   return 0; // Success
}

//// Manejo del comando STAT
void handle_stat(Client *client) {
   size_t size = 0, count = 0;

   for (Mailfile *mail = client->mails; mail->mailid[0]; mail++) {
       if (!mail->deleted) {
           size += mail->size;
           count++;
       }
   }

   char buffer[MAX_POP3_RESPONSE_LENGTH];
   snprintf(buffer, sizeof(buffer), OK_RESPONSE("%zu %zu"), count, size);
   send(client->socket_fd, buffer, strlen(buffer), 0);
   return;
}

void parse_command(char * buf, int socket) {
    return;
}

void handle_list(Client * client){
    return;
}
void handle_quit(Client * client){
    return;
}
void handle_dele(Client * client,char * buffer){
    return;
}
void handle_retr(Client * client,char * buffer){
    return;
}
