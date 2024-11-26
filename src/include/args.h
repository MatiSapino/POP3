#ifndef ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8
#define ARGS_H_kFlmYm1tW9p5npzDr2opQJ9jM8

#include <stdbool.h>
#include "user.h"

struct doh {
    char           *host;
    char           *ip;
    unsigned short  port;
    char           *path;
    char           *query;
};

struct pop3_args {
    char * pop3_addr;
    unsigned short pop3_port;
    char * mng_addr;
    unsigned short mng_port;
};

struct users {
    char *name;
    char *pass;
};


struct socks5args {
    char           *socks_addr;
    unsigned short  socks_port;

    char *          maildir;

    char *          mng_addr;
    unsigned short  mng_port;

    bool            disectors_enabled;

    struct doh      doh;
    struct users    users[MAX_USERS];
};

/**
 * Interpreta la linea de comandos (argc, argv) llenando
 * args con defaults o la seleccion humana. Puede cortar
 * la ejecución.
 */
void parse_args(const int argc, char **argv, struct pop3_args *args);

#endif

