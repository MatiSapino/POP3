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
    .handle_block = NULL
};

static void handle_get_metrics(struct admin_client *client);
static void handle_admin_command(struct admin_client *client, char *command);
static void handle_transform_add(struct admin_client *client, const char *name, const char *command);
static void handle_transform_list(struct admin_client *client);

void admin_read(struct selector_key *key) {
    fprintf(stderr, "Recibiendo comando administrativo\n");
    struct admin_client *client = (struct admin_client *)key->data;
    size_t limit;
    uint8_t *ptr = buffer_write_ptr(&client->read_buffer, &limit);
    ssize_t count = read(key->fd, ptr, limit);

    if (count <= 0) {
        selector_unregister_fd(key->s, key->fd);
        return;
    }

    buffer_write_adv(&client->read_buffer, count);

    // Procesar comando
    char command[ADMIN_BUFFER_SIZE];
    size_t command_len = 0;
    uint8_t *read_ptr = buffer_read_ptr(&client->read_buffer, &limit);
    
    while (limit > 0 && command_len < sizeof(command) - 1) {
        if (*read_ptr == '\n' || *read_ptr == '\r') {
            command[command_len] = '\0';
            handle_admin_command(client, command);
            buffer_read_adv(&client->read_buffer, command_len + 1);
            break;
        }
        command[command_len++] = *read_ptr++;
        limit--;
    }

    selector_set_interest(key->s, key->fd, OP_WRITE);
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

    selector_status status = selector_register(selector, admin_socket, &admin_handler, OP_READ, NULL);
    if (status != SELECTOR_SUCCESS) {
        perror("Failed to register admin socket");
        close(admin_socket);
        return;
    }
}

void admin_accept(struct selector_key *key) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    int client_fd = accept(key->fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        return;
    }
    
    struct admin_client *client = malloc(sizeof(*client));
    if (client == NULL) {
        close(client_fd);
        return;
    }
    
    memset(client, 0, sizeof(*client));
    buffer_init(&client->read_buffer, sizeof(client->read_buffer_data), client->read_buffer_data);
    buffer_init(&client->write_buffer, sizeof(client->write_buffer_data), client->write_buffer_data);
    
    selector_status status = selector_register(key->s, client_fd, &admin_handler, OP_READ, client);
    if (status != SELECTOR_SUCCESS) {
        free(client);
        close(client_fd);
    }

    // Enviar mensaje de bienvenida
    const char *welcome = "Bienvenido al servidor administrativo\n> ";
    write(client_fd, welcome, strlen(welcome));
} 