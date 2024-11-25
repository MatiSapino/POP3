#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "../include/transform.h"
#include "../include/user.h"

static struct transform_manager manager;

void transform_init(void) {
    memset(&manager, 0, sizeof(manager));
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

bool transform_set_enabled(const char *name, bool enabled) {
    for (size_t i = 0; i < manager.transform_count; i++) {
        if (strcmp(manager.transforms[i].name, name) == 0) {
            manager.transforms[i].enabled = enabled;
            return true;
        }
    }
    return false;
}

static bool apply_single_transform(const char *command, const char *input_file, const char *output_file) {
    pid_t pid = fork();
    
    if (pid == -1) {
        return false;
    }
    
    if (pid == 0) { // Child process
        // Redirigir entrada estándar desde input_file
        FILE *input = fopen(input_file, "r");
        if (input == NULL) {
            exit(1);
        }
        dup2(fileno(input), STDIN_FILENO);
        fclose(input);
        
        // Redirigir salida estándar hacia output_file
        FILE *output = fopen(output_file, "w");
        if (output == NULL) {
            exit(1);
        }
        dup2(fileno(output), STDOUT_FILENO);
        fclose(output);
        
        // Ejecutar el comando
        execl("/bin/sh", "sh", "-c", command, NULL);
        exit(1);
    }
    
    // Parent process
    int status;
    waitpid(pid, &status, 0);
    
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

bool transform_apply(const char *input_file, const char *output_file) {
    char temp_file[PATH_MAX];
    const char *current_input = input_file;
    
    for (size_t i = 0; i < manager.transform_count; i++) {
        if (!manager.transforms[i].enabled) {
            continue;
        }
        
        // Crear nombre de archivo temporal para esta transformación
        snprintf(temp_file, sizeof(temp_file), "%s.%zu.tmp", output_file, i);
        
        // Aplicar la transformación
        if (!apply_single_transform(manager.transforms[i].command, 
                                  current_input, 
                                  i == manager.transform_count - 1 ? output_file : temp_file)) {
            return false;
        }
        
        // Si no es la última transformación, el output se convierte en el próximo input
        if (i < manager.transform_count - 1) {
            current_input = temp_file;
        }
    }
    
    // Limpiar archivos temporales
    for (size_t i = 0; i < manager.transform_count - 1; i++) {
        snprintf(temp_file, sizeof(temp_file), "%s.%zu.tmp", output_file, i);
        unlink(temp_file);
    }
    
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