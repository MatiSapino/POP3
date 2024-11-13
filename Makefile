# Compiler and flags
CC = gcc
CFLAGS = -Wall -fdiagnostics-color=always -Iinclude

# Debug and optimization settings
ifneq ($(DEBUG), 0)
CFLAGS += -g -DDEVELOPMENT
else
CFLAGS += -O2
endif

# Source files
SRC = $(wildcard utils/*.c) main.c args.c
OBJ = $(SRC:.c=.o)

# Target executable
EXEC = ../../dist/server

# Default target
all: log $(EXEC)

log:
	@echo
	@echo "\033[0;36mMAKEFILE SERVER\033[0m"
	@echo "\tSOURCES=$(SRC)"
	@echo

# Build the executable
$(EXEC): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ) -lm

# Rule to compile .c files into .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up object files and executable
clean:
	rm -f $(OBJ) $(EXEC)

.PHONY: all clean
