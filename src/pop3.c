#include "include/pop3.h"
#include "include/commandParse.h"
#include "include/pop3_commands.h"
#include "include/metrics.h"
#include "include/netutils.h"

// allow to work on macOS
#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0x4000
#endif

ssize_t send_metrics(int fd, const void * buff, size_t n, int flags) {
  ssize_t count = send(fd, buff, n, flags);
  metrics_bytes_sent(count);
  return count;
}

static unsigned welcomeClient(struct selector_key * selector_key) {
    struct Client * client = selector_key->data;

    size_t limit;
    ssize_t count;
    uint8_t * buffer;
    selector_status status;

    buffer = buffer_read_ptr(&client->outputBuffer, &limit);
    count = send_metrics(selector_key->fd, buffer, limit, MSG_NOSIGNAL);

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

void errResponse(struct Client * client, const char * message) {
    size_t limit;
    uint8_t * buffer;
    ssize_t count;

    buffer = buffer_write_ptr(&client->outputBuffer, &limit);
    count = snprintf((char *) buffer, limit, "-ERR %s\r\n", message);
    buffer_write_adv(&client->outputBuffer, count);
}

void okResponse(struct Client * client, const char * message) {
    fprintf(stderr, "Starting okResponse\n");
    size_t limit;
    char * buffer = (char *)buffer_write_ptr(&client->outputBuffer, &limit);
    
    ssize_t count = snprintf(buffer, limit, "+OK %s\r\n", message);
    buffer_write_adv(&client->outputBuffer, count);
    fprintf(stderr, "okResponse written to buffer\n");
}


void response(struct Client * client, const char * message) {
    size_t limit;
    uint8_t * buffer;
    ssize_t count;

    buffer = buffer_write_ptr(&client->outputBuffer, &limit);
    count = snprintf((char *) buffer, limit, "%s\r\n", message);
    buffer_write_adv(&client->outputBuffer, count);
}

static enum pop3_state parseInput(struct selector_key * selector_key, struct Client * client) {
    enum pop3_state state;
    while (buffer_can_read(&client->inputBuffer) && buffer_fits(&client->outputBuffer, BUFFER_SPACE)) {
        enum commandState commandState = valid_command(&client->inputBuffer, client->commandParse);

        if (commandState == CMD_OK) {
            fprintf(stderr, "Command is OK\n");
            state = executeCommand(selector_key, client->commandParse->command);
            if(state==STATE_CLOSE){
                fprintf(stderr, "command is ok and state is close\n");    
            }
            reset_commandParser(client->commandParse);
            return state;
        }

        if (commandState == CMD_INVALID) {
            fprintf(stderr, "Command is invalid\n");

            errResponse(client, "Invalid command");
            return STATE_WRITE;
        }
    }

    return STATE_READ;
}

static unsigned readCommand(struct selector_key * selector_key) {
    struct Client * client = selector_key->data;

    if (client == NULL) {
        fprintf(stderr, "client is NULL in readCommand\n");
        return STATE_ERROR;
    }
    if (client->user == NULL) {
        fprintf(stderr, "client->user is NULL in readCommand\n");
    }

    size_t limit;
    ssize_t count;
    uint8_t * buffer;
    selector_status status;
    enum pop3_state states;
   
    buffer = buffer_write_ptr(&client->inputBuffer, &limit);
    count = recv(selector_key->fd, buffer, limit, MSG_NOSIGNAL);
   
    fprintf(stderr, "buffer copiado\n");
   
    if (count <= 0 && limit != 0) {
        fprintf(stderr, "me voy a handle error\n");
        goto handle_error;
    }

    buffer_write_adv(&client->inputBuffer, count);
    states = parseInput(selector_key, client);
    switch (states) {
        case STATE_WRITE:
            status = selector_set_interest_key(selector_key, OP_WRITE);
            if (status != SELECTOR_SUCCESS) {
                goto handle_error;
            }
            return STATE_WRITE;
        case STATE_ERROR:
            goto handle_error;
        default:
            return states;
    }

handle_error:
    status = selector_set_interest_key(selector_key, OP_WRITE);
    if (status != SELECTOR_SUCCESS) {
        selector_set_interest_key(selector_key, OP_NOOP);
    }

    return STATE_ERROR;
}

static unsigned writeToClient(struct selector_key * selector_key) {
    struct Client * client = selector_key->data;

    size_t limit;
    ssize_t count;
    uint8_t * buffer;
    selector_status status;
    enum pop3_state states;

    buffer = buffer_read_ptr(&client->outputBuffer, &limit);
    count = send_metrics(selector_key->fd, buffer, limit, MSG_NOSIGNAL);

    if (count <= 0 && limit != 0) {
        goto handle_error;
    }

    buffer_read_adv(&client->outputBuffer, count);

    if (client->closed) {
        return STATE_CLOSE;
    }

    if (buffer_can_read(&client->outputBuffer)) {
        return STATE_WRITE;
    }

    states = parseInput(selector_key, client);

    if (states == STATE_READ) {
        status = selector_set_interest_key(selector_key, OP_READ);
        if (status != SELECTOR_SUCCESS) {
            goto handle_error;
        }
        return STATE_READ;
    }
    if (states == STATE_ERROR) {
        goto handle_error;
    }
    return states;

handle_error:

    selector_set_interest_key(selector_key, OP_NOOP);
    return STATE_ERROR;
}

static unsigned int writeToFile(struct selector_key * selector_key) {
    struct Client * client = selector_key->data;

    size_t limit;
    ssize_t count;
    uint8_t * buffer;
    selector_status status;
    enum pop3_state states;

    buffer = buffer_read_ptr(&client->outputBuffer, &limit);
    count = send_metrics(selector_key->fd, buffer, limit, MSG_NOSIGNAL);

    if (count < 0 && limit != 0) {
        goto handle_error;
    }

    buffer_read_adv(&client->outputBuffer, count);

    if (buffer_can_read(&client->outputBuffer)) {
        return STATE_FILE_WRITE;
    }

    // TODO: email send to user needs to be here

    status = selector_set_interest_key(selector_key, OP_READ);
    if (status != SELECTOR_SUCCESS) {
        goto handle_error;
    }
    status = selector_set_interest_key(selector_key, OP_NOOP);
    if (status != SELECTOR_SUCCESS) {
        goto handle_error;
    }
    return STATE_FILE_WRITE;

    handle_error:
    selector_set_interest_key(selector_key, OP_NOOP);
    return STATE_ERROR;
}

static void stopWriteToFile(const enum pop3_state states, struct selector_key * selector_key) {
    struct Client * client = selector_key->data;
    selector_unregister_fd(selector_key->s, selector_key->fd);

    // TODO: need to add the finalization of the email
}

static const struct state_definition client_states[] = {
    {
        .state = STATE_WELCOME,
        .on_write_ready = welcomeClient,
    },
    {
        .state = STATE_READ,
        .on_read_ready = readCommand,
    },
    {
        .state = STATE_WRITE,
        .on_write_ready = writeToClient,
    },
    {
        .state = STATE_FILE_WRITE,
        .on_write_ready = writeToFile,
        .on_departure = stopWriteToFile,
    },
    {
        .state = STATE_CLOSE,
    },
    {
        .state = STATE_ERROR,
        .on_read_ready = writeToClient,
    }
};

static void closeConnection(struct selector_key * key) {
    struct Client * client = key->data;

    // TODO: this should close connection for auth user, do the other thing for unauth
    sockaddr_to_human_buffered((struct sockaddr*)&client->addr);

    if (client->commandParse != NULL) {
        free_commandParser(client->commandParse);
    }

    if (key->fd != -1) {
        fprintf(stderr, "key->fd !=-1\n");     
        selector_unregister_fd(key->s, key->fd);    
        fprintf(stderr, "unregistered\n");
        close(key->fd);
    }
    fprintf(stderr, "sali del if\n");
    free(client);
    metrics_connection_closed();
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
    struct state_machine * stm = &((struct Client *) selector_key->data)->stm;
    const enum pop3_state state = stm_handler_read(stm, selector_key);
    if (state == STATE_CLOSE) {
        closeConnection(selector_key);
    }
}

void pop3Write(struct selector_key * selector_key) {
    struct state_machine * stm = &((struct Client *) selector_key->data)->stm;
    const enum pop3_state state = stm_handler_write(stm, selector_key);
    if (state == STATE_ERROR || state == STATE_CLOSE) {
        closeConnection(selector_key);
    }
}

void pop3Block(struct selector_key * selector_key) {
    struct state_machine * stm = &((struct Client *) selector_key->data)->stm;
    const enum pop3_state state = stm_handler_block(stm, selector_key);
    if (state == STATE_ERROR || state == STATE_CLOSE) {
        closeConnection(selector_key);
    }
}

void pop3Close(struct selector_key * selector_key) {
    struct state_machine * stm = &((struct Client *) selector_key->data)->stm;
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
    
    client->user = malloc(sizeof(struct user));

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
    client->user = malloc(sizeof(struct user));
    client->commandParse = commandParseInit();
    client->authenticated = false;
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
    
    if (selectorStatus != SELECTOR_SUCCESS){
        goto handle_error;
    }

    metrics_new_connection();

    return;

handle_error:
    if (clientSocket != -1) {
        close(clientSocket);
    }

    if (client != NULL) {
        free(client);
    }

}