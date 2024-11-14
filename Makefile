include ./Makefile.inc

SERVER_SOURCES = $(wildcard src/server/utils/*.c)
MAIN_SOURCES = $(wildcard *.c)

OUTPUT_FOLDER = ./bin
OBJECTS_FOLDER = ./obj

SERVER_OBJECTS = $(SERVER_SOURCES:src/server/utils/%.c=obj%.o)
MAIN_OBJECTS = $(MAIN_SOURCES:%.c=obj%.o)

SERVER_OUTPUT_FILE = $(OUTPUT_FOLDER)/output_server
MAIN_OUTPUT_FILE = $(OUTPUT_FOLDER)/main

all: server main

server: $(SERVER_OUTPUT_FILE)
main: $(MAIN_OUTPUT_FILE)

$(SERVER_OUTPUT_FILE): $(SERVER_OBJECTS)
	mkdir -p $(OUTPUT_FOLDER)
	$(COMPILER) &(CFLAGS) $(SERVER_OBJECTS) -o $(SERVER_OUTPUT_FILE)

$(MAIN_OUTPUT_FILE): $(MAIN_OBJECTS)
	mkdir -p $(OUTPUT_FOLDER)
	$(COMPILER) $(CFLAGS) $(MAIN_OBJECTS) -o $(MAIN_OUTPUT_FILE)

obj%.o: src/server/utils/%.c
	mkdir -p $(OBJECTS_FOLDER)/output_server
	mkdir -p $(OBJECTS_FOLDER)/main
	$(COMPILER) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(OUTPUT_FOLDER)
	rm -rf $(OBJECTS_FOLDER)

.PHONY: all server main clean