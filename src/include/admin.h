#ifndef ADMIN_H
#define ADMIN_H

#include "selector.h"
#include "buffer.h"

#define ADMIN_BUFFER_SIZE 4096

enum admin_state {
    ADMIN_READ,
    ADMIN_WRITE,
    ADMIN_ERROR,
    ADMIN_CLOSE
};

struct admin_client {
    int fd;
    struct buffer read_buffer;
    struct buffer write_buffer;
    uint8_t read_buffer_data[ADMIN_BUFFER_SIZE];
    uint8_t write_buffer_data[ADMIN_BUFFER_SIZE];
    bool closed;
};

// Funciones públicas
void admin_init(const char *admin_addr, unsigned short admin_port, fd_selector selector);
void admin_write(struct selector_key *key);
void admin_close(struct selector_key *key);
void admin_read(struct selector_key *key);

#endif 