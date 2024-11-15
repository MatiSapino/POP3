#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <asm-generic/errno-base.h>
#include <unistd.h>
#include <sys/socket.h>
#include "src/server/include/selector.h"
#include "src/server/include/args.h"
#include "src/server/include/user.h"
#include <netinet/in.h>
#include <poll.h>
#include <arpa/inet.h>

static int setupSocket(char * addr, int port);
static void handleError(int newSocket);

extern struct pop3_args pop3_args;
int serverSocket = -1;




static struct pollfd fds[MAX_CLIENTS + 1];

static bool done = false;

static void sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}


void read_socket(int socket, char *buf, size_t size ) {

    fds[0].fd = socket;
    fds[0].events = POLLIN;
    int nfds = 1;

    struct selector_key key = {(fd_selector)poll(fds, nfds, -1), socket, buf};

    struct Client * client = create_user(socket, buf);

    stm_init(client->stm);

    if (client == NULL) {
        perror("Failed to create client");
        return;
    }

    while (1) {
        ssize_t bytes_read = read(socket, buf, size);
        if (bytes_read <= 0) {
            if (bytes_read == 0) {
                fprintf(stderr, "Connection closed by client\n");
            } else {
                perror("Error reading from socket");
            }
            return;
        }
        stm_parse(buf, &key, client);
        // fprintf(stderr, "%s", buf);
        send(socket, buf, strlen(buf), 0);
    }
}

int main(const int argc, const char **argv) {
    parse_args(argc, argv, &pop3_args);

    close(STDIN_FILENO);

    int ret = -1;

    serverSocket = setupSocket(
        (pop3_args.pop3_addr == NULL ? "0.0.0.0" : pop3_args.pop3_addr),
        pop3_args.pop3_port
    )

    if (serverSocket == -1) {
        goto finally;
    }

    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);

    const fd_handler passiveHandler = {
        .handle_read = passiveAccept,
        .handle_write = NULL,
        .handle_block = NULL,
        .handle_close = NULL
    }
    

    unsigned port = 1080;

    if (argc == 1) {
        // use default
    } else if (argc == 2) {
        char *end = 0;
        const long sl = strtol(argv[1], &end, 10);

        if (end == argv[1] || '\0' != *end
           || ((LONG_MIN == sl || LONG_MAX == sl) && ERANGE == errno)
           || sl < 0 || sl > USHRT_MAX) {
            fprintf(stderr, "port should be an integer: %s\n", argv[1]);
            return 1;
           }
        port = sl;
    } else {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    // nothing to read in stdin
    close(STDIN_FILENO);

    //pop_init(NULL);
    const char *err_msg = NULL;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    const int server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server < 0) {
        err_msg = "unable to create socket";
        goto finally;
    }

    fprintf(stdout, "Listening on TCP port %d\n", port);

    // man 7 ip. dont matter report if fails.
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &(int){ 1 }, sizeof(int));

    //Asocia el socket con el puerto especificado.
    if (bind(server, (struct sockaddr*) &addr, sizeof(addr)) < 0) {
        err_msg = "unable to bind socket";
        goto finally;
    }

    //El socket espera conexiones entrantes, hasta 20 en la cola
    if (listen(server, 20) < 0) {
        err_msg = "unable to listen";
        goto finally;
    }

    // registrar sigterm es útil para terminar el programa normalmente.
    // esto ayuda mucho en herramientas como valgrind.
    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);

    //Se corre esto siempre esperando conexiones al socket que creamos antes
    //selector_select() espera conexiones
    for(;!done;) {
        socklen_t addr_len = sizeof(addr);
        int new_socket = accept(server, (struct sockaddr*) &addr, &addr_len);
        char buf[100];
        read_socket(new_socket, buf, sizeof(addr));

        if (new_socket >= 0) {
            fprintf(stdout, "Connection established\n");
        }
        if (new_socket < 0) {
            err_msg = "Unable to accept";
            goto finally;
        }
        err_msg = NULL;
    }

    if (err_msg == NULL) {
        err_msg = "closing";
    }

    int ret = 0;

    finally:
         if (err_msg) {
            perror(err_msg);
            ret = 1;
        }

        // socksv5_pool_destroy();

        if (server >= 0) {
            close(server);
        }
        return ret;
}

static int setupSocket(char * addr, int port) {

    struct sockaddr_in serverAddr;
    int newSocket = -1;

    if (port < 0) {
        fprintf(stderr, "Invalid port number: %d\n", port);
        return -1;
    }

    newSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    if (newSocket < 0) {
        fprintf(stderr, "Failed to create socket\n");
        handleError(newSocket);
        return -1;
    }

    if (setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        fprintf(stderr, "Failed to set socket options\n");
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, addr, &serverAddr.sin_addr) < 0) {
        fprintf(stderr, "Invalid address/ Address not supported\n");
        handleError(newSocket);
        return -1;
    }

    if (bind(newSocket, (struct sockaddr *)&serverAddr, sizeof(serverAddr))) {
        fprintf(stderr, "Failed to bind socket\n");
        handleError(newSocket);
        return -1;
    }

    if (listen(newSocket, 20)) {
        fprintf(stderr, "Failed to listen on socket\n");
        handleError(newSocket);
        return -1;
    }

    fprintf(stdout, "Listening on %s:%d\n", addr, port);

    return newSocket;
}

static void handleError(int newSocket) {
    if (newSocket != -1) {
        close(newSocket);
    }
    fprintf(stderr, "Error: %s\n", strerror(errno));
}