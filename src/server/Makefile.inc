CC := gcc

CFLAGS := --std=c11 -Wall -pedantic -pedantic-errors -Wformat=2 -Wextra -Wno-unused-parameter -Wundef -Wuninitialized -Wno-implicit-fallthrough -D_POSIX_C_SOURCE=200809L -fsanitize=address -pthread -g -O3

SRC_DIR :=src

BIN_DIR := ../..