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
    for (size_t i = 0; i < manager.transform_count; i++) {
        if (strcmp(manager.transforms[i].name, name) == 0) {
            return &manager.transforms[i];
        }
    }
    return NULL;
}

bool transform_test(const char *name, const char *input, char *output, size_t output_size) {
    // Buscar la transformación por nombre
    struct transform *t = find_transform(name);
    if (t == NULL) {
        return false;
    }

    // Crear un pipe para la comunicación
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        return false;
    }

    // Fork para ejecutar el comando
    pid_t pid = fork();
    if (pid == -1) {
        close(pipefd[0]);
        close(pipefd[1]);
        return false;
    }

    if (pid == 0) {  // Proceso hijo
        close(pipefd[0]);  // Cerrar extremo de lectura
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);

        // Ejecutar el comando
        execlp("sh", "sh", "-c", t->command, NULL);
        exit(1);
    }

    // Proceso padre
    close(pipefd[1]);  // Cerrar extremo de escritura
    
    // Escribir input al pipe
    write(pipefd[0], input, strlen(input));
    close(pipefd[0]);

    // Leer resultado
    ssize_t bytes_read = read(pipefd[0], output, output_size - 1);
    if (bytes_read < 0) {
        return false;
    }
    output[bytes_read] = '\0';

    // Esperar que termine el hijo
    int status;
    waitpid(pid, &status, 0);

    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
} 