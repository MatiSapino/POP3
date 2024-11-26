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
#include <errno.h>

#define LOG_FILE "/var/log/pop3_access.log"

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
static void handle_transform_test(struct admin_client *client, const char *name, const char *input);
static void handle_config_command(struct admin_client *client, char *args);
static void handle_transform_command(struct admin_client *client, char *args);
static void handle_user_history(struct admin_client *client, const char *username);

static struct server_config current_config = {
    .select_timeout = {.tv_sec = 10, .tv_nsec = 0},
    .buffer_size = 4096,
    .max_connections = 1000,
    .verbose_logging = false
};

void admin_read(struct selector_key *key) {
    // Si es el socket principal (listening socket)
    if (key->data == NULL) {
        // Este es el socket de escucha, aceptar nueva conexión
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        const int client_fd = accept(key->fd, (struct sockaddr*)&client_addr, &client_addr_len);
        if (client_fd < 0) {
            return;
        }
        
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
            free(client);
            close(client_fd);
            return;
        }
    } else {
        // Es un socket de cliente, manejar la lectura de datos
        struct admin_client *client = (struct admin_client *)key->data;
        
        size_t space;
        uint8_t *ptr = buffer_write_ptr(&client->read_buffer, &space);
        
        if (!buffer_can_write(&client->read_buffer)) {
            selector_unregister_fd(key->s, key->fd);
            admin_close(key);
            return;
        }
        
        ssize_t n = read(key->fd, ptr, space);
        
        if (n <= 0) {
            selector_unregister_fd(key->s, key->fd);
            admin_close(key);
            return;
        }
        
        buffer_write_adv(&client->read_buffer, n);
        
        size_t count;
        uint8_t *read_ptr = buffer_read_ptr(&client->read_buffer, &count);
        
        if(count > 0) {
            for(size_t i = 0; i < count; i++) {
                if(read_ptr[i] == '\n') {
                    read_ptr[i] = '\0';
                    handle_admin_command(client, (char *)read_ptr);
                    buffer_read_adv(&client->read_buffer, i + 1);
                    
                    selector_status st = selector_set_interest(key->s, key->fd, OP_WRITE);
                    
                    if(st != SELECTOR_SUCCESS) {
                        selector_unregister_fd(key->s, key->fd);
                        admin_close(key);
                    }
                    break;
                }
            }
        }
    }
}

void admin_write(struct selector_key *key) {
    struct admin_client *client = (struct admin_client *)key->data;
    
    size_t limit;
    uint8_t *ptr = buffer_read_ptr(&client->write_buffer, &limit);

    ssize_t count = write(key->fd, ptr, limit);
    
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
    }
    else if (strcmp(cmd, "history") == 0) {
        char *username = strtok(NULL, " ");
        handle_user_history(client, username);
    }
    else if (strcmp(cmd, "transform") == 0) {
        handle_transform_command(client, strtok(NULL, ""));
    }
    else if (strcmp(cmd, "config") == 0) {
        handle_config_command(client, strtok(NULL, ""));
    }
    else if (strcmp(cmd, "help") == 0) {
        const char *help_text =
            "Available commands:\n"
            "  metrics              - Show server metrics\n"
            "  history <username>   - Show command history for a specific user\n"
            "  transform list       - List available transformations\n"
            "  transform add <name> <cmd> - Add new transformation\n"
            "  transform test <name> <input> - Test transformation\n"
            "  transform enable <name> - Enable transformation\n"
            "  transform disable <name> - Disable transformation\n"
            "  config get          - Show current configuration\n"
            "  config set timeout <sec[.nsec]> - Set select timeout\n"
            "  config set buffer_size <bytes> - Set I/O buffer size\n"
            "  config set max_connections <num> - Set max connections\n"
            "  config set verbose <true|false> - Enable/disable verbose logging\n"
            "  help                - Show this help\n";
        write_string_to_buffer(&client->write_buffer, help_text);
    }
    else {
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

static void handle_transform_test(struct admin_client *client, const char *name, const char *input) {
    char output[ADMIN_BUFFER_SIZE];
    if (transform_test(name, input, output, sizeof(output))) {
        char response[ADMIN_BUFFER_SIZE];
        snprintf(response, sizeof(response), "Transform result: %s\n", output);
        write_string_to_buffer(&client->write_buffer, response);
    } else {
        write_string_to_buffer(&client->write_buffer, "Transform test failed\n");
    }
}

static void handle_config_command(struct admin_client *client, char *args) {
    char *subcmd = strtok(args, " ");
    if (subcmd == NULL) {
        write_string_to_buffer(&client->write_buffer, "Usage: config <get|set> [parameter] [value]\n");
        return;
    }

    if (strcmp(subcmd, "get") == 0) {
        char response[ADMIN_BUFFER_SIZE];
        snprintf(response, sizeof(response),
            "Current Configuration:\n"
            "  timeout: %ld.%09ld seconds\n"
            "  buffer_size: %zu bytes\n"
            "  max_connections: %u\n"
            "  verbose_logging: %s\n",
            current_config.select_timeout.tv_sec,
            current_config.select_timeout.tv_nsec,
            current_config.buffer_size,
            current_config.max_connections,
            current_config.verbose_logging ? "enabled" : "disabled");
        write_string_to_buffer(&client->write_buffer, response);
    } 
    else if (strcmp(subcmd, "set") == 0) {
        char *param = strtok(NULL, " ");
        char *value = strtok(NULL, " ");
        
        if (!param || !value) {
            write_string_to_buffer(&client->write_buffer, "Usage: config set <parameter> <value>\n");
            return;
        }

        if (strcmp(param, "timeout") == 0) {
            char *dot = strchr(value, '.');
            long seconds, nanoseconds = 0;
            
            if (dot) {
                *dot = '\0';
                seconds = atol(value);
                nanoseconds = atol(dot + 1);
            } else {
                seconds = atol(value);
            }

            if (config_set_timeout(seconds, nanoseconds)) {
                write_string_to_buffer(&client->write_buffer, "Timeout updated successfully\n");
            } else {
                write_string_to_buffer(&client->write_buffer, "Failed to update timeout\n");
            }
        }
        else if (strcmp(param, "buffer_size") == 0) {
            size_t size = atol(value);
            if (config_set_buffer_size(size)) {
                write_string_to_buffer(&client->write_buffer, "Buffer size updated successfully\n");
            } else {
                write_string_to_buffer(&client->write_buffer, "Failed to update buffer size\n");
            }
        }
        else if (strcmp(param, "max_connections") == 0) {
            unsigned int max = atoi(value);
            if (config_set_max_connections(max)) {
                write_string_to_buffer(&client->write_buffer, "Max connections updated successfully\n");
            } else {
                write_string_to_buffer(&client->write_buffer, "Failed to update max connections\n");
            }
        }
        else if (strcmp(param, "verbose") == 0) {
            bool enabled = strcmp(value, "true") == 0 || strcmp(value, "1") == 0;
            if (config_set_verbose(enabled)) {
                write_string_to_buffer(&client->write_buffer, "Verbose logging updated successfully\n");
            } else {
                write_string_to_buffer(&client->write_buffer, "Failed to update verbose logging\n");
            }
        }
        else {
            write_string_to_buffer(&client->write_buffer, "Unknown parameter\n");
        }
    }
    else {
        write_string_to_buffer(&client->write_buffer, "Unknown config command\n");
    }
}

static void handle_transform_command(struct admin_client *client, char *args) {
    char *subcmd = strtok(args, " ");
    if (subcmd == NULL) {
        write_string_to_buffer(&client->write_buffer, 
            "Usage: transform <list|add|test|enable|disable> [args...]\n");
        return;
    }

    if (strcmp(subcmd, "list") == 0) {
        handle_transform_list(client);
    }
    else if (strcmp(subcmd, "add") == 0) {
        char *name = strtok(NULL, " ");
        char *cmd = strtok(NULL, "");  // Obtiene el resto de la línea como comando
        
        if (!name || !cmd) {
            write_string_to_buffer(&client->write_buffer, 
                "Usage: transform add <name> <command>\n");
            return;
        }
        
        // Eliminar espacios al inicio del comando
        while (*cmd == ' ') cmd++;
        
        handle_transform_add(client, name, cmd);
    }
    else if (strcmp(subcmd, "test") == 0) {
        char *name = strtok(NULL, " ");
        char *input = strtok(NULL, "");  // Obtiene el resto como input
        
        if (!name || !input) {
            write_string_to_buffer(&client->write_buffer, 
                "Usage: transform test <name> <input_text>\n");
            return;
        }
        
        // Eliminar espacios al inicio del input
        while (*input == ' ') input++;
        
        handle_transform_test(client, name, input);
    }
    else if (strcmp(subcmd, "enable") == 0 || strcmp(subcmd, "disable") == 0) {
        char *name = strtok(NULL, " ");
        if (!name) {
            write_string_to_buffer(&client->write_buffer, 
                "Usage: transform enable/disable <name>\n");
            return;
        }
        
        bool enable = (strcmp(subcmd, "enable") == 0);
        if (transform_set_enabled(name, enable)) {
            write_string_to_buffer(&client->write_buffer, 
                enable ? "Transform enabled\n" : "Transform disabled\n");
        } else {
            write_string_to_buffer(&client->write_buffer, 
                "Transform not found\n");
        }
    }
    else {
        write_string_to_buffer(&client->write_buffer, 
            "Unknown transform command. Use: list, add, test, enable, disable\n");
    }
}

static void handle_user_history(struct admin_client *client, const char *username) {
    if (username == NULL) {
        write_string_to_buffer(&client->write_buffer, "Usage: history <username>\n");
        return;
    }

    FILE *log = fopen(LOG_FILE, "r");
    if (log == NULL) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), 
                "Error: Could not open log file: %s\n", 
                strerror(errno));
        write_string_to_buffer(&client->write_buffer, error_msg);
        fprintf(stderr, "DEBUG: %s", error_msg);
        return;
    }

    fprintf(stderr, "DEBUG: Archivo log abierto correctamente\n");

    char line[1024];
    char response[ADMIN_BUFFER_SIZE] = "";
    size_t total_len = 0;
    
    while (fgets(line, sizeof(line), log)) {
        if (strstr(line, username) != NULL) {
            size_t line_len = strlen(line);
            if (total_len + line_len < ADMIN_BUFFER_SIZE - 1) {
                strcat(response, line);
                total_len += line_len;
                fprintf(stderr, "DEBUG: Línea encontrada para %s: %s", username, line);
            } else {
                fprintf(stderr, "DEBUG: Buffer lleno, deteniendo lectura\n");
                break;
            }
        }
    }
    
    fclose(log);

    if (total_len == 0) {
        snprintf(response, sizeof(response), "No commands found for user: %s\n", username);
        fprintf(stderr, "DEBUG: No se encontraron comandos para el usuario %s\n", username);
    }

    write_string_to_buffer(&client->write_buffer, response);
}

bool config_set_timeout(long seconds, long nanoseconds) {
    if (seconds < 0 || nanoseconds < 0 || nanoseconds >= 1000000000) {
        return false;
    }
    
    current_config.select_timeout.tv_sec = seconds;
    current_config.select_timeout.tv_nsec = nanoseconds;
    
    // Actualizar el timeout en el selector
    // selector_set_timeout(key->s, current_config.select_timeout);
    return true;
}

bool config_set_buffer_size(size_t size) {
    if (size < 1024 || size > 1048576) { // Entre 1KB y 1MB
        return false;
    }
    current_config.buffer_size = size;
    return true;
}

bool config_set_max_connections(unsigned int max) {
    if (max < 1 || max > 10000) {
        return false;
    }
    current_config.max_connections = max;
    return true;
}

bool config_set_verbose(bool enabled) {
    current_config.verbose_logging = enabled;
    return true;
}

void config_get_current(struct server_config *config) {
    memcpy(config, &current_config, sizeof(struct server_config));
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
        close(admin_socket);
        return;
    }
}
