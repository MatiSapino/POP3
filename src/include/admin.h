#ifndef ADMIN_H
#define ADMIN_H

#include "selector.h"
#include "buffer.h"

#define ADMIN_BUFFER_SIZE 4096
#define MAX_CONFIG_VALUE_SIZE 256

enum admin_state {
    ADMIN_READ,
    ADMIN_WRITE,
    ADMIN_ERROR,
    ADMIN_CLOSE
};

struct server_config {
    struct timespec select_timeout;  // Timeout para select
    size_t buffer_size;             // Tamaño de buffer para I/O
    unsigned int max_connections;    // Máximo de conexiones concurrentes
    bool verbose_logging;           // Logging detallado
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

// Funciones de configuración
bool config_set_timeout(long seconds, long nanoseconds);
bool config_set_buffer_size(size_t size);
bool config_set_max_connections(unsigned int max);
bool config_set_verbose(bool enabled);
void config_get_current(struct server_config *config);

#endif 