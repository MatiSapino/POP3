#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "../include/admin.h"
#include "../include/metrics.h"
#include "../include/buffer.h"
#include "../include/transform.h"

static int admin_socket = -1;

static const struct fd_handler admin_handler = {
    .handle_read = admin_read,
    .handle_write = admin_write,
    .handle_close = admin_close,
    .handle_block = NULL,
};

static void handle_get_metrics(struct admin_client *client);
static void handle_admin_command(struct admin_client *client, char *command);
static void handle_transform_add(struct admin_client *client, const char *name, const char *command);
static void handle_transform_list(struct admin_client *client);
static void admin_accept(struct selector_key *key);

static void admin_read(struct selector_key *key) {
    printf("DEBUG: Iniciando admin_read\n");

    // Si es el socket principal (listening socket)
    if (key->data == NULL) {
        // Este es el socket de escucha, aceptar nueva conexión
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        const int client_fd = accept(key->fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            fprintf(stderr, "DEBUG: Error en accept. client_fd = %d\n", client_fd);
            return;
        }
        
        printf("DEBUG: Nueva conexión administrativa aceptada. FD: %d\n", client_fd);
        
        // Crear y configurar el cliente
        struct admin_client *client = malloc(sizeof(*client));
        if (client == NULL) {
            close(client_fd);
            return;
        }
        
        // Inicializar cliente y buffers
        memset(client, 0, sizeof(*client));
        client->fd = client_fd;
        client->closed = false;
        buffer_init(&client->read_buffer, ADMIN_BUFFER_SIZE, client->read_buffer_data);
        buffer_init(&client->write_buffer, ADMIN_BUFFER_SIZE, client->write_buffer_data);
        
        // Hacer el socket no bloqueante
        int flags = fcntl(client_fd, F_GETFL, 0);
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
        
        // Registrar para lectura
        selector_status status = selector_register(key->s, client_fd, &admin_handler, OP_READ, client);
        if (status != SELECTOR_SUCCESS) {
            printf("DEBUG: Error registrando cliente: %d\n", status);
            free(client);
            close(client_fd);
            return;
        }
        
        printf("DEBUG: Cliente registrado exitosamente\n");
    } else {
        // Es un socket de cliente, manejar la lectura de datos
        struct admin_client *client = (struct admin_client *)key->data;
        
        size_t space = buffer_available_space(&client->read_buffer);
        uint8_t *ptr = buffer_write_ptr(&client->read_buffer, &space);
        
        ssize_t n = read(key->fd, ptr, space);
        if (n <= 0) {
            // Error o conexión cerrada
            selector_unregister_fd(key->s, key->fd);
            admin_close(key);
            return;
        }
        
        buffer_write_adv(&client->read_buffer, n);
        printf("DEBUG: Datos leídos del cliente: %.*s\n", (int)n, ptr);
        
        // Aquí procesarías el comando recibido
    }
}

void admin_write(struct selector_key *key) {
    fprintf(stderr, "Enviando respuesta administrativa\n");
    struct admin_client *client = (struct admin_client *)key->data;
    size_t limit;
    uint8_t *ptr = buffer_read_ptr(&client->write_buffer, &limit);

    ssize_t count = write(key->fd, ptr, limit);
    if (count < 0) {
        selector_unregister_fd(key->s, key->fd);
        return;
    }

    buffer_read_adv(&client->write_buffer, count);

    if (!buffer_can_read(&client->write_buffer)) {
        if (client->closed) {
            selector_unregister_fd(key->s, key->fd);
        } else {
            selector_set_interest(key->s, key->fd, OP_READ);
        }
    }
}

void admin_close(struct selector_key *key) {
    struct admin_client *client = (struct admin_client *)key->data;
    if (client != NULL) {
        free(client);
    }
}

static void write_string_to_buffer(struct buffer *buffer, const char *str) {
    size_t len = strlen(str);
    size_t limit;
    uint8_t *write_ptr = buffer_write_ptr(buffer, &limit);
    
    size_t to_write = len < limit ? len : limit;
    memcpy(write_ptr, str, to_write);
    buffer_write_adv(buffer, to_write);
}

static void handle_admin_command(struct admin_client *client, char *command) {
    char *cmd = strtok(command, " ");
    if (cmd == NULL) {
        write_string_to_buffer(&client->write_buffer, "Invalid command\n");
        return;
    }

    if (strcmp(cmd, "metrics") == 0) {
        handle_get_metrics(client);
    } else if (strcmp(cmd, "transform") == 0) {
        char *subcmd = strtok(NULL, " ");
        if (subcmd == NULL) {
            write_string_to_buffer(&client->write_buffer, "Invalid transform command\n");
            return;
        }

        if (strcmp(subcmd, "list") == 0) {
            handle_transform_list(client);
        } else if (strcmp(subcmd, "add") == 0) {
            char *name = strtok(NULL, " ");
            char *cmd = strtok(NULL, "");
            if (name && cmd) {
                handle_transform_add(client, name, cmd);
            } else {
                write_string_to_buffer(&client->write_buffer, "Usage: transform add <name> <command>\n");
            }
        } else {
            write_string_to_buffer(&client->write_buffer, "Unknown transform command\n");
        }
    } else {
        write_string_to_buffer(&client->write_buffer, "Unknown command\n");
    }
}

static void handle_get_metrics(struct admin_client *client) {
    struct metrics current_metrics;
    metrics_get_metrics(&current_metrics);
    
    char response[ADMIN_BUFFER_SIZE];
    snprintf(response, sizeof(response),
        "Current Connections: %zu\n"
        "Total Connections: %zu\n"
        "Bytes Sent: %zu\n"
        "Bytes Received: %zu\n"
        "Total Commands: %zu\n"
        "Failed Commands: %zu\n"
        "Active Users: %zu\n",
        current_metrics.currentConnections,
        current_metrics.totalConnections,
        current_metrics.bytesSent,
        current_metrics.bytesReceived,
        current_metrics.totalCommands,
        current_metrics.failedCommands,
        current_metrics.activeUsers);
    
    write_string_to_buffer(&client->write_buffer, response);
}

static void handle_transform_add(struct admin_client *client, const char *name, const char *command) {
    if (transform_add(name, command)) {
        write_string_to_buffer(&client->write_buffer, "Transform added successfully\n");
    } else {
        write_string_to_buffer(&client->write_buffer, "Failed to add transform\n");
    }
}

static void handle_transform_list(struct admin_client *client) {
    char buffer[ADMIN_BUFFER_SIZE];
    transform_list(buffer, sizeof(buffer));
    write_string_to_buffer(&client->write_buffer, buffer);
}

static void admin_accept(struct selector_key *key) {
    fprintf(stderr, "DEBUG: Iniciando admin_accept\n");
    
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);
    
    const int client_fd = accept(key->fd, (struct sockaddr*)&client_addr, &client_addr_len);
    if (client_fd < 0) {
        fprintf(stderr, "DEBUG: Error en accept. client_fd = %d\n", client_fd);
        return;
    }
    
    // Crear y configurar el cliente
    struct admin_client *client = malloc(sizeof(*client));
    if (client == NULL) {
        close(client_fd);
        return;
    }
    
    // Inicializar el cliente y sus buffers
    memset(client, 0, sizeof(*client));
    client->fd = client_fd;
    client->closed = false;
    
    // Inicializar los buffers
    buffer_init(&client->read_buffer, ADMIN_BUFFER_SIZE, client->read_buffer_data);
    buffer_init(&client->write_buffer, ADMIN_BUFFER_SIZE, client->write_buffer_data);
    
    // Hacer el socket no bloqueante
    int flags = fcntl(client_fd, F_GETFL, 0);
    fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
    
    // Registrar para lectura
    selector_status status = selector_register(key->s, client_fd, &admin_handler, OP_READ, client);
    if (status != SELECTOR_SUCCESS) {
        free(client);
        close(client_fd);
        return;
    }
    
    fprintf(stderr, "DEBUG: Cliente administrativo conectado\n");
}

void admin_init(const char *admin_addr, unsigned short admin_port, fd_selector selector) {
    struct sockaddr_in addr;
    
    admin_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (admin_socket < 0) {
        perror("admin socket creation failed");
        return;
    }
    
    int opt = 1;
    setsockopt(admin_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(admin_addr);
    addr.sin_port = htons(admin_port);
    
    if (bind(admin_socket, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("admin socket bind failed");
        close(admin_socket);
        return;
    }
    
    if (listen(admin_socket, 1) < 0) {
        perror("admin socket listen failed");
        close(admin_socket);
        return;
    }

    // Hacer el socket no bloqueante
    int flags = fcntl(admin_socket, F_GETFL, 0);
    fcntl(admin_socket, F_SETFL, flags | O_NONBLOCK);

    // Registrar el socket principal para lectura
    selector_status status = selector_register(selector, admin_socket, &admin_handler, OP_READ, NULL);
    if (status != SELECTOR_SUCCESS) {
        fprintf(stderr, "Failed to register admin socket. Error: %d\n", status);
        close(admin_socket);
        return;
    }

    fprintf(stderr, "Admin server listening on %s:%d\n", admin_addr, admin_port);
}
