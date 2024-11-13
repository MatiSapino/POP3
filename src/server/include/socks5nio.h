#ifndef SOCKS5NIO_H
#define SOCKS5NIO_H

#include <stdint.h>     // uint8_t
#include <netinet/in.h> // sockaddr_storage
#include "buffer.h"
#include "selector.h"
// #include "request.h"

// Define the maximum number of states
#define MAX_POOL 10

// SOCKS v5 state machine states
enum socks_v5state {
    HELLO_READ,
    HELLO_WRITE,
    REQUEST_READ,
    REQUEST_WRITE,
    CONNECTING,
    FORWARDING,
    DONE,
    ERROR,
};

// Definition of the hello state structure
struct hello_st {
    buffer *rb, *wb;             // Buffers for reading and writing
    struct hello_parser parser;  // Hello parser
    uint8_t method;              // Selected authentication method
};

// Definition of the socks5 state structure
struct socks5 {
    int client_fd;
    int origin_fd;
    struct sockaddr_storage client_addr;
    socklen_t client_addr_len;
    struct addrinfo *origin_resolution;
    struct state_machine stm;

    union {
        struct hello_st hello;
        struct request_st request;
        struct copy copy;
    } client;

    union {
        struct connecting conn;
        struct copy copy;
    } orig;

    struct socks5 *next;
    unsigned references;
};

// External variables and pool management
extern struct socks5 *pool;
extern unsigned int pool_size;
extern const unsigned int max_pool;

// Function declarations
struct socks5 *socks5_new(int client_fd);
void socks5_destroy(struct socks5 *s);
void socksv5_pool_destroy(void);

// State handlers for the SOCKSv5 proxy
void socksv5_read(struct selector_key *key);
void socksv5_write(struct selector_key *key);
void socksv5_block(struct selector_key *key);
void socksv5_close(struct selector_key *key);
void socksv5_passive_accept(struct selector_key *key);
void socksv5_done(struct selector_key *key);

#endif // SOCKS5NIO_H
