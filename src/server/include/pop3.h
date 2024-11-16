#ifndef POP_H
#define POP_H

#include <sys/stat.h>
#include <netutils.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "stm.h"
#include "user.h"
#include "selector.h"

#define MAX_POP3_ARG_LENGTH 40
#define MAX_USERNAME_LENGTH MAX_POP3_ARG_LENGTH
#define MAX_PASSWORD_LENGTH MAX_POP3_ARG_LENGTH
#define MAX_POP3_RESPONSE_LENGTH 512
#define MAX_CLIENT_MAILS 0x1000
#define MAX_CLIENTS 500
#define BUFFER_SIZE 1024
#define OK_RESP "+OK"
#define ERR_RESP "-ERR"
#define ENTER "\r\n"

typedef struct Mailfile {
    char mailid[71];
    bool deleted;
    size_t size;
} Mailfile;

typedef struct Client {
    struct sockaddr_storage addr;
    struct buffer inputBuffer;
    uint8_t inputBufferData[BUFFER_SIZE];
    struct buffer outputBuffer;
    uint8_t outputBufferData[BUFFER_SIZE];
    struct state_machine stm;

    struct commandParse * commandParse;

    struct user * user;


    // char username[MAX_USERNAME_LENGTH + 1];
    // bool authenticated;
    // Mailfile mails[MAX_CLIENT_MAILS];
    // int socket_fd;
} Client;

enum pop3_state {
    STATE_WELCOME,
    STATE_READ,
    STATE_WRITE,
    STATE_FILE_WRITE,
    STATE_CLOSE,
    STATE_ERROR,
};

/*
Inicializa el servidor, configura el directorio donde están los archivos de correo, 
si no existe lo crea. Crea el vector con los datos de conexión de clientes. 
*/
// void pop_init(char * dir_name);

void passiveAccept(struct selector_key *key);

// int initialize_pop_connection(int sock_fd, struct sockaddr_in client_address);
// void handle_stat(Client *client);
// void handle_list(Client * client);
// void handle_quit(Client * client);
// void handle_dele(Client * client,char * buffer);
// void handle_retr(Client * client,char * buffer);

void okResponse(struct Client * client, const char * message);


#endif