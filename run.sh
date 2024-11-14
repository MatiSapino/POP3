#!/bin/bash

# Limpiar compilaciones anteriores
make clean

# Compilar el proyecto
make

# Navegar al directorio donde está el binario y ejecutar el programa con el argumento 8080
cd build/bin/
./my_program 8080
