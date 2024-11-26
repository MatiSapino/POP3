#include <stdio.h>     /* for printf */
#include <stdlib.h>    /* for exit */
#include <limits.h>    /* LONG_MIN et al */
#include <string.h>    /* memset */
#include <errno.h>
#include <getopt.h>
#include "../include/args.h"

struct pop3_args pop3_args;

static unsigned short port(const char *s) {
     char *end     = 0;
     const long sl = strtol(s, &end, 10);

     if (end == s|| '\0' != *end
        || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
        || sl < 0 || sl > USHRT_MAX) {
         fprintf(stderr, "port should in in the range of 1-65536: %s\n", s);
         exit(1);
         return 1;
     }
     return (unsigned short)sl;
}

static void user(char *s, struct users *user) {
    char *p = strchr(s, ':');
    if(p == NULL) {
        fprintf(stderr, "password not found\n");
        exit(1);
    } else {
        *p = 0;
        p++;
        //Aca se rompe 
        user->name = s;
        user->pass = p;
    }

}

static void version(void) {
    fprintf(stderr, "POP3 version 1.0\n"
                    "ITBA Protocolos de Comunicación 2024 -- Grupo 7\n"
                    "La licencia nos pertenece\n");
}

/*

-> maildir tree .
.
└── user1
    ├── cur
    ├── new
    │     └── mail1
    └── tmp

*/

static void usage(const char *progname) {
    fprintf(stderr,
        "Usage: %s [OPTION]...\n"
        "\n"
        "   -h               Imprime la ayuda y termina.\n"
        "   -l <POP3 addr>   Dirección donde servirá el proxy POP.\n"
        "   -L <conf  addr>  Dirección donde servirá el servicio de management.\n"
        "   -p <POP3 port>   Puerto entrante conexiones POP3.\n"
        "   -P <conf port>   Puerto entrante conexiones configuracion\n"
        "   -u <name>:<pass> Usuario y contraseña de usuario que puede usar el servidor. Hasta 10.\n"
        "   -v               Imprime información sobre la versión versión y termina.\n"
        "   -d               Carpeta donde residen los Maildirs"
        "\n",
        progname);
    exit(1);
}

void parse_args(const int argc, char **argv, struct pop3_args *args) {
    memset(args, 0, sizeof(*args)); // sobre todo para setear en null los punteros de users
    struct users usr = {0};

    // Valores por defecto
    args->pop3_addr = "0.0.0.0";
    args->pop3_port = 1080;
    args->mng_addr = "127.0.0.1";  // Solo acceso local por defecto
    args->mng_port = 9090;         // Puerto por defecto para administración

    int c;

    while (true) {
        c = getopt(argc, argv, "hl:L:Np:P:u:vd");
        if (c == -1)
            break;

        switch (c) {
            case 'h':
                usage(argv[0]);
                break;
            case 'l':
                args->pop3_addr = optarg;
                break;
            case 'L':
                args->mng_addr = optarg;
                break;
            case 'p':
                args->pop3_port = port(optarg);
                break;
            case 'd':
                  print_maildir();
                  break;
            case 'P':
                args->mng_port   = port(optarg);
                break;
            case 'u':
                user(optarg,&usr);
                add_user(usr.name,usr.pass);
                break;
            case 'v':
                version();
                exit(0);
                break;
            default:
                fprintf(stderr, "unknown argument %d.\n", c);
                exit(1);
        }
    }

    if (optind < argc - 1) {
        fprintf(stderr, "argument not accepted: ");
        while (optind < argc - 1) {
            fprintf(stderr, "%s ", argv[optind++]);
        }
        fprintf(stderr, "\n");
        exit(1);
    }

}
