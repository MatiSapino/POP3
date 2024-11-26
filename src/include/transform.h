#ifndef TRANSFORM_H
#define TRANSFORM_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_TRANSFORM_NAME 32
#define MAX_TRANSFORM_CMD 256
#define MAX_TRANSFORMS 10

struct transform {
    char name[MAX_TRANSFORM_NAME];
    char command[MAX_TRANSFORM_CMD];
    bool enabled;
};

struct transform_manager {
    struct transform transforms[MAX_TRANSFORMS];
    size_t transform_count;
};

// Inicializa el sistema de transformaciones
void transform_init(void);

// Agrega una nueva transformación
bool transform_add(const char *name, const char *command);

// Habilita/deshabilita una transformación
bool transform_set_enabled(const char *name, bool enabled);

// Aplica las transformaciones habilitadas a un archivo
bool transform_apply(const char *input_file, const char *output_file);

// Lista las transformaciones disponibles
void transform_list(char *buffer, size_t buffer_size);

// Agregar esta declaración junto con las otras funciones de transform
bool transform_test(const char *name, const char *input, char *output, size_t output_size);

#endif 