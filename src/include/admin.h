#ifndef ADMIN_H
#define ADMIN_H

#include <stddef.h>
#include <stdbool.h>
#include "selector.h"
#include "buffer.h"

#define ADMIN_BUFFER_SIZE 1024
#define MAX_ADMIN_ARGS 3

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

void admin_init(const char *admin_addr, unsigned short admin_port, fd_selector selector);
void admin_accept(struct selector_key *key);
void admin_close(struct selector_key *key);
void admin_read(struct selector_key *key);
void admin_write(struct selector_key *key);

#endif 