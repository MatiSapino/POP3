# Makefile principal

include Makefile.inc

SRC_DIR = src/server
BUILD_DIR = build
OBJ_DIR = $(BUILD_DIR)/objects
BIN_DIR = $(BUILD_DIR)/bin
INCLUDE_DIR = $(SRC_DIR)/include

SRCS = $(wildcard $(SRC_DIR)/utils/*.c) args.c main.c
OBJS = $(SRCS:%.c=$(OBJ_DIR)/%.o)

EXECUTABLE = $(BIN_DIR)/my_program

CFLAGS += -I$(INCLUDE_DIR)

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

$(OBJ_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)

.PHONY: all clean test
