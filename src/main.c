#include <errno.h>
#include <limits.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include "include/selector.h"
#include "include/args.h"
#include "include/user.h"
#include <netinet/in.h>
#include <poll.h>
#include <arpa/inet.h>
#include "include/metrics.h"
#include "include/admin.h"

#define SELECTOR_SIZE 1024

static int setupSocket(char * addr, int port);
static void handleError(int newSocket);

extern struct pop3_args pop3_args;
int serverSocket = -1;

bool done = false;

static void sigterm_handler(const int signal) {
    printf("signal %d, cleaning up and exiting\n",signal);
    done = true;
}

int main(const int argc, char **argv) {
    
    if(!set_maildir()){
        fprintf(stderr, "Couldn't create maildir");
        goto finally;
    }
    
    parse_args(argc, argv, &pop3_args);

    close(STDIN_FILENO);

    metrics_init();

    transform_init();

    int ret = -1;

    const struct selector_init init = {
        .signal = SIGALRM,
        .select_timeout = {
            .tv_sec = 10,
            .tv_nsec = 0,
        },
    };

    fd_selector selector = NULL;
    selector_status selectStatus = selector_init(&init);
    if (selectStatus != SELECTOR_SUCCESS) {
        goto finally;
    }

    serverSocket = setupSocket(
        pop3_args.pop3_addr == NULL ? "::" : pop3_args.pop3_addr,
        pop3_args.pop3_port
    );
    if (serverSocket == -1) {
        goto finally;
    }

    signal(SIGTERM, sigterm_handler);
    signal(SIGINT,  sigterm_handler);

    if (serverSocket != -1 && selector_fd_set_nio(serverSocket) == -1) {
        goto finally;
    }

    selector = selector_new(SELECTOR_SIZE);
    if (selector == NULL) {
        goto finally;
    }
    
    admin_init(pop3_args.mng_addr, pop3_args.mng_port, selector);
    
    const fd_handler passiveHandler = {
        .handle_read = passiveAccept,
        .handle_write = NULL,
        .handle_block = NULL,
        .handle_close = NULL
    };
    
    selectStatus = selector_register(
        selector,
        serverSocket,
        &passiveHandler,
        OP_READ,
        NULL
    );
    if (selectStatus != SELECTOR_SUCCESS) {
        goto finally;
    }

    while (!done) {
        selectStatus = selector_select(selector);
        if (selectStatus != SELECTOR_SUCCESS) {
            goto finally;
        }
    }

    ret = 0;

    finally:
        if (selectStatus != SELECTOR_SUCCESS) {
            fprintf(stderr, "Error: %s\n", selector_error(selectStatus));
            if (selectStatus == SELECTOR_IO) {
                fprintf(stderr, "Errno: %s\n", strerror(errno));
            }
        }
        if (selector != NULL) {
            selector_destroy(selector);
        }
        selector_close();
        if (serverSocket >= 0) {
            close(serverSocket);
        }
        
        return ret;
}

static int setupSocket(char * addr, int port) {

    struct sockaddr_in6 serverAddr;
    int newSocket = -1;

    if (port < 0) {
        fprintf(stderr, "Invalid port number: %d\n", port);
        return -1;
    }

    newSocket = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);

    if (newSocket < 0) {
        fprintf(stderr, "Failed to create socket\n");
        handleError(newSocket);
        return -1;
    }

    if (setsockopt(newSocket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        fprintf(stderr, "Failed to set socket options\n");
    }

    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin6_family = AF_INET6;
    serverAddr.sin6_addr = in6addr_any;
    serverAddr.sin6_port = htons(port);

    if (inet_pton(AF_INET6, addr, &serverAddr.sin6_addr) < 0) {
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