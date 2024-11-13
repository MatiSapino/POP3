#include "pop3.h"

struct command_function {
    char *name;
    enum pop3_state (*function)(struct selector_key *key, struct command *command);
};

static enum pop3_state handle_user(struct selector_key *key, struct command *command);
static enum pop3_state handle_pass(struct selector_key *key, struct command *command);
static enum pop3_state handle_quit(struct selector_key *key, struct command *command);
static enum pop3_state handle_capa(struct selector_key *key, struct command *command);


static struct command_function nonauthCommands[] = {
        {"USER",    handle_user},
        {"PASS",    handle_pass},
        {"QUIT",    handle_quit},
        {"CAPA",    handle_capa},
        {NULL,      NULL},
};


static const char* maildir;
static Connection connections[MAX_CLIENTS] = {0}; 
#define OK_RESPONSE(m) OK_RESP " " m ENTER
#define ERR_RESPONSE(m) ERR_RESP " " m ENTER


// Función para generar una respuesta OK
const char* ok_respon(const char *message) {
    static char response[MAX_POP3_RESPONSE_LENGTH];
    snprintf(response, sizeof(response), "%s %s%s", OK_RESP, message, ENTER);
    return response;
}

// Función para generar una respuesta de error
const char* err_respon(const char *message) {
    static char response[MAX_POP3_RESPONSE_LENGTH];
    snprintf(response, sizeof(response), "%s %s%s", ERR_RESP, message, ENTER);
    return response;
}

enum pop3_state execute_command(struct selector_key *key, char *command) {
    struct Connection *client = key->data;
    struct command_function *commandFunction;
    int i;

    //commandFunction = data->authenticated ? authCommands : nonauthCommands;
    commandFunction = nonauthCommands;

    for (i = 0; commandFunction[i].name != NULL; i++) {
        if (strcasecmp(commandFunction[i].name, command->data) == 0) {
            return commandFunction[i].function(key, command);
        }
    }

    errResponse(key->data, "Command not recognized");

    command_parser_reset(data->commandParser);

    return POP3_WRITE;
}

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

//manejo del comando USER
static pop3_state handle_user(Connection *client, const char *username, char **response){
    if (!check_username(username, maildir) || !check_user(username, maildir)){
        *response = ERR_RESPONSE("Invalid username");
        return POP3_WRITE;
    }
    strcpy(client->username, username);
    *response = OK_RESPONSE("User accepted");
    return POP3_WRITE;
}

//manejo del comando PASS
static pop3_state handle_pass(Connection *client, const char *pass, char **response){
    if (!check_password(client->username, pass, maildir)){
        client->username[0] = '\0';
        *response = ERR_RESPONSE(" Invalid password");
        return POP3_WRITE;
    }
    client->authenticated = true;
    *response = OK_RESPONSE("Authentication successful");
    return POP3_WRITE;
}
////

// Manejo del comando STAT
static size_t handle_stat(Connection *client, char **response) {
    size_t size = 0, count = 0;

    for (Mailfile *mail = client->mails; mail->uid[0]; mail++) {
        if (!mail->deleted) {
            size += mail->size;
            count++;
        }
    }

    char buffer[MAX_POP3_RESPONSE_LENGTH];
    snprintf(buffer, sizeof(buffer), OK_RESPONSE("%zu %zu"), count, size);
    *response = strdup(buffer);
    return strlen(*response);
}

