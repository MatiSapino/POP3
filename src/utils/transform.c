#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include "../include/transform.h"
#include "../include/user.h"

static struct transform_manager manager;

void transform_init(void) {
    memset(&manager, 0, sizeof(manager));
    
    // Agregar transformación por defecto (cat)
    transform_add("cat", "cat");
    
    // Si spamassassin está instalado, lo agregamos
    if (system("which spamassassin > /dev/null 2>&1") == 0) {
        transform_add("spamassassin", "spamassassin");
    }
    
    // Si renattach está instalado, lo agregamos
    if (system("which renattach > /dev/null 2>&1") == 0) {
        transform_add("renattach", "renattach");
    }
}

bool transform_add(const char *name, const char *command) {
    fprintf(stderr, "DEBUG: Agregando transformación '%s' con comando '%s'\n", name, command);
    if (manager.transform_count >= MAX_TRANSFORMS) {
        return false;
    }
    
    struct transform *t = &manager.transforms[manager.transform_count];
    
    strncpy(t->name, name, MAX_TRANSFORM_NAME - 1);
    t->name[MAX_TRANSFORM_NAME - 1] = '\0';
    
    strncpy(t->command, command, MAX_TRANSFORM_CMD - 1);
    t->command[MAX_TRANSFORM_CMD - 1] = '\0';
    
    t->enabled = true;
    manager.transform_count++;
    
    return true;
}

static bool apply_single_transform(const char *command, int input_fd, int output_fd) {
    pid_t pid = fork();
    
    if (pid == -1) {
        fprintf(stderr, "Fork failed: %s\n", strerror(errno));
        return false;
    }
    
    if (pid == 0) { // Child process
        // Redirigir entrada estándar
        if (dup2(input_fd, STDIN_FILENO) == -1) {
            fprintf(stderr, "Failed to redirect stdin: %s\n", strerror(errno));
            exit(1);
        }
        
        // Redirigir salida estándar
        if (dup2(output_fd, STDOUT_FILENO) == -1) {
            fprintf(stderr, "Failed to redirect stdout: %s\n", strerror(errno));
            exit(1);
        }
        
        // Cerrar los file descriptors originales
        close(input_fd);
        close(output_fd);
        
        // Ejecutar el comando
        execl("/bin/sh", "sh", "-c", command, NULL);
        fprintf(stderr, "Exec failed: %s\n", strerror(errno));
        exit(1);
    }
    
    // Parent process
    int status;
    waitpid(pid, &status, 0);
    
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool transform_apply(const char *input_file, const char *output_file) {
    int input_fd = open(input_file, O_RDONLY);
    if (input_fd == -1) {
        fprintf(stderr, "Cannot open input file: %s\n", strerror(errno));
        return false;
    }
    
    char temp_path[PATH_MAX];
    const char *current_input = input_file;
    int current_input_fd = input_fd;
    
    for (size_t i = 0; i < manager.transform_count; i++) {
        if (!manager.transforms[i].enabled) {
            continue;
        }
        
        // Crear archivo temporal para esta transformación
        snprintf(temp_path, sizeof(temp_path), "%s.%zu.tmp", output_file, i);
        
        // Abrir archivo de salida
        int output_fd = open(temp_path, 
                           O_WRONLY | O_CREAT | O_TRUNC, 
                           S_IRUSR | S_IWUSR);
        
        if (output_fd == -1) {
            fprintf(stderr, "Cannot open output file: %s\n", strerror(errno));
            close(current_input_fd);
            return false;
        }
        
        // Aplicar la transformación
        bool success = apply_single_transform(manager.transforms[i].command, 
                                           current_input_fd, 
                                           output_fd);
        
        close(output_fd);
        
        if (current_input_fd != input_fd) {
            close(current_input_fd);
            unlink(current_input);  // Eliminar archivo temporal anterior
        }
        
        if (!success) {
            fprintf(stderr, "Transform failed: %s\n", manager.transforms[i].name);
            unlink(temp_path);
            return false;
        }
        
        // Preparar para la siguiente iteración
        current_input = temp_path;
        current_input_fd = open(current_input, O_RDONLY);
        if (current_input_fd == -1) {
            fprintf(stderr, "Cannot open intermediate file: %s\n", strerror(errno));
            return false;
        }
    }
    
    // Mover el último archivo temporal al archivo de salida final
    if (rename(current_input, output_file) == -1) {
        fprintf(stderr, "Cannot rename final file: %s\n", strerror(errno));
        close(current_input_fd);
        return false;
    }
    
    close(current_input_fd);
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

static struct transform* find_transform(const char *name) {
    fprintf(stderr, "DEBUG: Buscando transformación '%s'\n", name);
    for (size_t i = 0; i < manager.transform_count; i++) {
        if (strcmp(manager.transforms[i].name, name) == 0) {
            return &manager.transforms[i];
        }
    }
    return NULL;
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