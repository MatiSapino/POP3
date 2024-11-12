#ifndef POP_H
#define POP_H

#include <sys/stat.h>
#include <netutils.h>


#include <stdbool.h>
#include <stddef.h>

#define MAX_POP3_ARG_LENGTH 40
#define MAX_USERNAME_LENGTH MAX_POP3_ARG_LENGTH
#define MAX_PASSWORD_LENGTH MAX_POP3_ARG_LENGTH
#define MAX_POP3_RESPONSE_LENGTH 512
#define MAX_CLIENT_MAILS 0x1000
#define MAX_CLIENTS 500

typedef struct Mailfile {
    char uid[71];
    bool deleted;
    size_t size;
} Mailfile;

typedef struct Connection {
    char username[MAX_USERNAME_LENGTH + 1];
    bool authenticated;
    Mailfile mails[MAX_CLIENT_MAILS];
} Connection;
/*
Inicializa el servidor, configura el directorio donde están los archivos de correo, 
si no existe lo crea. Crea el vector con los datos de conexión de clientes. 
*/
void pop_init(char * dir_name);


#endif