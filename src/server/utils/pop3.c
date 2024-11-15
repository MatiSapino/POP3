#include "pop3.h"

//struct command_function {
//    char *name;
//    enum pop3_state (*function)(struct selector_key *key, struct command *command);
//};
///*

//
//
// static const char* maildir;
// static Client connections[MAX_CLIENTS] = {0};
// #define OK_RESPONSE(m) OK_RESP " " m ENTER
// #define ERR_RESPONSE(m) ERR_RESP " " m ENTER
// //
// //
// //// Función para generar una respuesta OK
// const char* ok_respon(const char *message) {
//    static char response[MAX_POP3_RESPONSE_LENGTH];
//    snprintf(response, sizeof(response), "%s %s%s", OK_RESP, message, ENTER);
//    return response;
// }

// //// Función para generar una respuesta de error
// const char* err_respon(const char *message) {
//    static char response[MAX_POP3_RESPONSE_LENGTH];
//    snprintf(response, sizeof(response), "%s %s%s", ERR_RESP, message, ENTER);
//    return response;
// }


// // Setea la ruta del directorio de mails, usando un valor por defecto si no se pasa un argumento.
// static void set_maildir(const char *dir) {
//    maildir = dir ? dir : "./dist/mail";
// }

// //// Crea el directorio de mails si no existe.
// static void create_maildir_if_needed() {
//    if (access(maildir, F_OK) == -1) {
//        mkdir(maildir, S_IRWXU);
//    }
// }

// static  void reset_connections() {
//    for (size_t i = 0; i < MAX_CLIENTS; i++) {
//        connections[i].username[0] = 0;
//        connections[i].authenticated = false;
//    }
// }

// void pop_init(char *dir) {
//    set_maildir(dir);
//    create_maildir_if_needed();
//    reset_connections();
// }

// int initialize_pop_connection(int sock_fd, struct sockaddr_in client_address)
// {
//    Client *client_connection = &connections[sock_fd];
//    client_connection->username[0] = '\0';
//    client_connection->authenticated = false;

//    const char welcome_msg[] = OK_RESPONSE(" POP3 server ready");
//    ssize_t sent_bytes = send(sock_fd, welcome_msg, strlen(welcome_msg), 0);

//    if (sent_bytes < 0)
//    {
//        fprintf(stderr, "Error sending welcome message to client\n");
//        return -1; // Error
//    }

//    return 0; // Success
// }

// //// Manejo del comando STAT
// void handle_stat(Client *client) {
//    size_t size = 0, count = 0;

//    for (Mailfile *mail = client->mails; mail->mailid[0]; mail++) {
//        if (!mail->deleted) {
//            size += mail->size;
//            count++;
//        }
//    }

//    char buffer[MAX_POP3_RESPONSE_LENGTH];
//    snprintf(buffer, sizeof(buffer), OK_RESPONSE("%zu %zu"), count, size);
//    send(client->socket_fd, buffer, strlen(buffer), 0);
//    return;
// }

// void parse_command(char * buf, int socket) {
//     return;
// }

// void handle_list(Client * client){
//     return;
// }
// void handle_quit(Client * client){
//     return;
// }
// void handle_dele(Client * client,char * buffer){
//     return;
// }
// void handle_retr(Client * client,char * buffer){
//     return;
// }

static unsigned welcomeClient(struct selector_key * selector_key) {
    struct Client * client = selector_key->data;

    size_t limit;
    ssize_t count;
    uint8_t * buffer;
    selector_status status;

    buffer = buffer_read_ptr(&client->outputBuffer, &limit);
    // TODO: check flags in send
    count = send(selector_key->fd, buffer, limit, 0);

    if (count <= 0) {
        goto handle_error;
    }

    buffer_read_adv(&client->outputBuffer, count);

    if (buffer_can_read(&client->outputBuffer)) {
        return STATE_WELCOME;
    }

    status = selector_set_interest_key(selector_key, OP_READ);
    if (status != SELECTOR_SUCCESS) {
        goto handle_error;
    }

    return STATE_READ;
    
handle_error:
    status = selector_set_interest_key(selector_key, OP_NOOP);
    return STATE_ERROR;
}

static const struct state_definition client_states[] = {
    {
        .state = STATE_WELCOME,
        .on_write_ready = welcomeClient,
    }
};

static void closeConnection(struct selector_key * key) {
    struct Client * client = key->data;

    if (key->fd != -1) {
        selector_unregister_fd(key->s, key->fd);
        close(key->fd);
    }

    free(client);
}

static void pop3Read(struct selector_key * selector_key);
static void pop3Write(struct selector_key * selector_key);
static void pop3Block(struct selector_key * selector_key);
static void pop3Close(struct selector_key * selector_key);

static const fd_handler clientHandler = {
    .handle_read = pop3Read,
    .handle_write = pop3Write,
    .handle_close = pop3Block,
    .handle_block = pop3Close,
};

void pop3Read(struct selector_key * selector_key) {
    struct state_machine * stm = &((struct Client *) key->data)->stm;
    const enum pop3_state state = stm_handler_read(stm, selector_key);
    if (state == STATE_CLOSE) {
        closeConnection(selector_key);
    }
}

void pop3Write(struct selector_key * selector_key) {
    struct state_machine * stm = &((struct Client *) key->data)->stm;
    const enum pop3_state state = stm_handler_write(stm, selector_key);
    if (state == STATE_ERROR || state == STATE_CLOSE) {
        closeConnection(selector_key);
    }
}

void pop3Block(struct selector_key * selector_key) {
    struct state_machine * stm = &((struct Client *) key->data)->stm;
    const enum pop3_state state = stm_handler_block(stm, selector_key);
    if (state == STATE_ERROR || state == STATE_CLOSE) {
        closeConnection(selector_key);
    }
}

void pop3Close(struct selector_key * selector_key) {
    struct state_machine * stm = &((struct Client *) key->data)->stm;
    stm_handler_close(stm, selector_key);
}

void passiveAccept(struct selector_key * key) {
    struct sockaddr_storage clientAddr;
    socklen_t clientAddrLen = sizeof(clientAddr);

    struct Client * client = NULL;

    const int clientSocket = accept(key->fd, (struct sockaddr *) &clientAddr, &clientAddrLen);
    if (clientSocket == -1) {
        goto handle_error;
    }

    client = calloc(1, sizeof(struct Client));
    if (client == NULL) {
        goto handle_error;
    }

    client->addr = clientAddr;

    buffer_init(&client->inputBuffer, BUFFER_SIZE, client->inputBufferData);
    buffer_init(&client->outputBuffer, BUFFER_SIZE, client->outputBufferData);

    char * s = "+OK POP3 server ready\r\n";

    for (int i = 0; s[i]; i++) {
        buffer_write(&client->outputBuffer, s[i]);
    }

    client->stm.initial = STATE_WELCOME;
    client->stm.max_state = STATE_ERROR;
    client->stm.states = client_states;

    stm_init(&client->stm);

    if (selector_fd_set_nio(clientSocket) == -1) {
        goto handle_error;
    }

    selector_status selectorStatus = selector_register(
        key->s,
        clientSocket,
        &clientHandler,
        OP_WRITE,
        client
    );
    if (selectorStatus != SELECTOR_SUCCESS) {
        goto handle_error;
    }

    return;

handle_error:
    if (clientSocket != -1) {
        close(clientSocket);
    }

    if (client != NULL) {
        free(client);
    }

}