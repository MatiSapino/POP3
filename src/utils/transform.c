#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include "../include/transform.h"
#include "../include/user.h"
#include "../include/pop3.h"

static struct transform_manager manager;

void transform_init(void) {
    memset(&manager, 0, sizeof(manager));
    
    // Agregar transformaciones por defecto
    transform_add("base64", "base64");
    transform_add("cat", "cat");
    transform_add("uppercase", "tr '[:lower:]' '[:upper:]'");
    
    fprintf(stderr, "DEBUG: Transformaciones inicializadas\n");
}

struct transform * find_transform(const char *name) {
    fprintf(stderr, "DEBUG: Buscando transformación '%s'\n", name);
    for (size_t i = 0; i < manager.transform_count; i++) {
        if (strcmp(manager.transforms[i].name, name) == 0) {
            fprintf(stderr, "DEBUG: Transformación '%s' encontrada\n", name);
            return &manager.transforms[i];
        }
    }
    fprintf(stderr, "DEBUG: Transformación '%s' no encontrada\n", name);
    return NULL;
}

bool transform_add(const char *name, const char *command) {
    fprintf(stderr, "DEBUG: Agregando transformación '%s' con comando '%s'\n", name, command);
    if (manager.transform_count >= MAX_TRANSFORMS) {
        fprintf(stderr, "DEBUG: Límite de transformaciones alcanzado\n");
        return false;
    }
    
    // Verificar si ya existe
    if (find_transform(name) != NULL) {
        fprintf(stderr, "DEBUG: La transformación '%s' ya existe\n", name);
        return false;
    }
    
    struct transform *t = &manager.transforms[manager.transform_count];
    
    strncpy(t->name, name, MAX_TRANSFORM_NAME - 1);
    t->name[MAX_TRANSFORM_NAME - 1] = '\0';
    
    strncpy(t->command, command, MAX_TRANSFORM_CMD - 1);
    t->command[MAX_TRANSFORM_CMD - 1] = '\0';
    
    t->enabled = true;
    manager.transform_count++;
    
    fprintf(stderr, "DEBUG: Transformación agregada exitosamente\n");
    return true;
}

bool transform_apply(const char *input_file, const char *output_file, const char *transform_name, struct Client *client) {
    fprintf(stderr, "DEBUG: Aplicando transformación '%s'\n", transform_name);
    
    struct transform *t = find_transform(transform_name);
    if (t == NULL || !t->enabled) {
        fprintf(stderr, "DEBUG: Transformación no encontrada o deshabilitada\n");
        errResponse(client, "transformation not found or disabled");
        return false;
    }

    // Crear pipes para la comunicación
    int input_pipe[2];
    int output_pipe[2];
    
    if (pipe(input_pipe) == -1 || pipe(output_pipe) == -1) {
        fprintf(stderr, "DEBUG: Error creando pipes: %s\n", strerror(errno));
        errResponse(client, "internal error");
        return false;
    }

    pid_t pid = fork();
    if (pid == -1) {
        fprintf(stderr, "DEBUG: Error en fork: %s\n", strerror(errno));
        errResponse(client, "internal error");
        return false;
    }

    if (pid == 0) {  // Proceso hijo
        // Configurar stdin/stdout
        close(input_pipe[1]);   // Cerrar extremo de escritura del input
        close(output_pipe[0]);  // Cerrar extremo de lectura del output
        
        dup2(input_pipe[0], STDIN_FILENO);
        dup2(output_pipe[1], STDOUT_FILENO);
        
        close(input_pipe[0]);
        close(output_pipe[1]);

        // Ejecutar el comando
        fprintf(stderr, "DEBUG: Hijo ejecutando comando: %s\n", t->command);
        execl("/bin/sh", "sh", "-c", t->command, NULL);
        fprintf(stderr, "DEBUG: Error ejecutando comando: %s\n", strerror(errno));
        exit(1);
    }

    // Proceso padre
    close(input_pipe[0]);   // Cerrar extremo de lectura del input
    close(output_pipe[1]);  // Cerrar extremo de escritura del output

    // Leer archivo de entrada
    FILE *input = fopen(input_file, "r");
    if (!input) {
        fprintf(stderr, "DEBUG: Error abriendo archivo de entrada: %s\n", strerror(errno));
        errResponse(client, "error reading input file");
        return false;
    }

    // Escribir contenido al pipe
    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        write(input_pipe[1], buffer, bytes_read);
    }
    fclose(input);
    close(input_pipe[1]);  // Cerrar pipe de entrada

    // Abrir archivo de salida
    FILE *output = fopen(output_file, "w");
    if (!output) {
        fprintf(stderr, "DEBUG: Error abriendo archivo de salida: %s\n", strerror(errno));
        errResponse(client, "error writing output file");
        close(output_pipe[0]);
        return false;
    }

    // Leer resultado del pipe
    while ((bytes_read = read(output_pipe[0], buffer, sizeof(buffer))) > 0) {
        fwrite(buffer, 1, bytes_read, output);
    }
    fclose(output);
    close(output_pipe[0]);

    // Esperar que termine el hijo
    int status;
    waitpid(pid, &status, 0);
    
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        fprintf(stderr, "DEBUG: Transformación falló con estado %d\n", WEXITSTATUS(status));
        errResponse(client, "transformation failed");
        unlink(output_file);  // Eliminar archivo de salida en caso de error
        return false;
    }

    fprintf(stderr, "DEBUG: Transformación completada exitosamente\n");
    return true;
}

void transform_list(char *buffer, size_t buffer_size) {
    size_t offset = 0;
    
    for (size_t i = 0; i < manager.transform_count && offset < buffer_size; i++) {
        offset += snprintf(buffer + offset, buffer_size - offset,
                         "%s: %s [%s]\n",
                         manager.transforms[i].name,
                         manager.transforms[i].command,
                         manager.transforms[i].enabled ? "enabled" : "disabled");
    }
}

bool transform_test(const char *name, const char *input, char *output, size_t output_size) {
    fprintf(stderr, "DEBUG: transform_test - name: '%s', input: '%s'\n", name, input);
    
    struct transform *t = find_transform(name);
    if (t == NULL) {
        fprintf(stderr, "DEBUG: Transformación '%s' no encontrada\n", name);
        return false;
    }
    fprintf(stderr, "DEBUG: Encontrada transformación '%s' con comando '%s'\n", name, t->command);

    int input_pipe[2];   // Padre -> Hijo
    int output_pipe[2];  // Hijo -> Padre
    
    if (pipe(input_pipe) == -1 || pipe(output_pipe) == -1) {
        fprintf(stderr, "DEBUG: Error creando pipes: %s\n", strerror(errno));
        return false;
    }

    pid_t pid = fork();
    if (pid == -1) {
        fprintf(stderr, "DEBUG: Error en fork: %s\n", strerror(errno));
        return false;
    }

    if (pid == 0) {  // Proceso hijo
        // Configurar stdin desde input_pipe
        close(input_pipe[1]);  // Cerrar extremo de escritura del input
        dup2(input_pipe[0], STDIN_FILENO);
        close(input_pipe[0]);

        // Configurar stdout hacia output_pipe
        close(output_pipe[0]);  // Cerrar extremo de lectura del output
        dup2(output_pipe[1], STDOUT_FILENO);
        close(output_pipe[1]);

        // Eliminar comillas extras del comando si existen
        char *cmd = strdup(t->command);
        if (cmd[0] == '\'' && cmd[strlen(cmd)-1] == '\'') {
            cmd[strlen(cmd)-1] = '\0';
            cmd++;
        }
        
        fprintf(stderr, "DEBUG: Hijo ejecutando comando: sh -c '%s'\n", cmd);
        execlp("sh", "sh", "-c", cmd, NULL);
        fprintf(stderr, "DEBUG: Error ejecutando comando: %s\n", strerror(errno));
        exit(1);
    }

    // Proceso padre
    close(input_pipe[0]);   // Cerrar extremo de lectura del input
    close(output_pipe[1]);  // Cerrar extremo de escritura del output

    // Remover comillas del input si existen
    char *clean_input = strdup(input);
    if (clean_input[0] == '\'' && clean_input[strlen(clean_input)-1] == '\'') {
        clean_input[strlen(clean_input)-1] = '\0';
        clean_input++;
    }
    
    // Escribir input
    ssize_t bytes_written = write(input_pipe[1], clean_input, strlen(clean_input));
    close(input_pipe[1]);  // Cerrar después de escribir
    fprintf(stderr, "DEBUG: Escritos %zd bytes al pipe\n", bytes_written);

    // Leer resultado
    ssize_t bytes_read = read(output_pipe[0], output, output_size - 1);
    close(output_pipe[0]);
    fprintf(stderr, "DEBUG: Leídos %zd bytes del pipe\n", bytes_read);
    
    if (bytes_read < 0) {
        fprintf(stderr, "DEBUG: Error leyendo del pipe: %s\n", strerror(errno));
        return false;
    }
    output[bytes_read] = '\0';

    int status;
    waitpid(pid, &status, 0);
    fprintf(stderr, "DEBUG: Proceso hijo terminó con estado %d\n", WEXITSTATUS(status));

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool transform_set_enabled(const char *name, bool enabled) {
    struct transform *t = find_transform(name);
    if (t == NULL) {
        return false;
    }
    t->enabled = enabled;
    return true;
}