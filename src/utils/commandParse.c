#include "commandParse.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

struct commandParse * commandParseInit() {
  struct commandParse * commandParse = malloc(sizeof(struct commandParse));
  if (commandParse == NULL) {
    return NULL;
  }
  
  commandParse->state = commandParse->prevState = CMD_DISPATCHER;
  commandParse->bytes = 0;
  commandParse->command = malloc(sizeof(struct command));
  if (commandParse->command == NULL) {
    free(commandParse);
    return NULL;
  }
  
  memset(commandParse->command->data, 0, CMD_MAX_LENGHT);
  commandParse->command->args1 = NULL;
  commandParse->command->args2 = NULL;

  return commandParse;
}

void reset_commandParser(struct commandParse * command_parse) {
  command_parse->state = command_parse->prevState = CMD_DISPATCHER;
  command_parse->bytes = 0;
  command_parse->command->args1 = command_parse->command->args2 = NULL;
}

static void reset_command_parser(struct commandParse * command_parse) {
  command_parse->bytes = 0;
  command_parse->command->args1 = command_parse->command->args2 = NULL;
}

void free_commandParser(struct commandParse * command_parse) {
  memset(command_parse->command, 0, sizeof(struct command));
  free(command_parse->command);
  memset(command_parse, 0, sizeof(struct commandParse));
  free(command_parse);
}

static enum commandState parse_command(struct commandParse * commandParse, uint8_t c) {
  fprintf(stderr, "Parsing char: '%c', bytes: %d, state: %d\n", c, commandParse->bytes, commandParse->state);
  
  // Verificar límite de buffer
  if (commandParse->bytes >= CMD_MAX_LENGHT) {
    fprintf(stderr, "ERROR: Buffer overflow prevented\n");
    return CMD_INVALID;
  }

  commandParse->command->data[commandParse->bytes] = c;
  commandParse->bytes++;

  if (commandParse->state == CMD_ARGS) {
    fprintf(stderr, "In CMD_ARGS state, prev_state: %d\n", commandParse->prevState);
    if (c != ' ' && c != '\t' && c != '\r' && commandParse->prevState == CMD_DISPATCHER) {
      if (commandParse->command->args1 == NULL) {
        commandParse->command->args1 = commandParse->command->data + (commandParse->bytes - 1);
        fprintf(stderr, "Setting args1 to offset: %ld\n", 
                (long)(commandParse->command->args1 - commandParse->command->data));
      } else {
        commandParse->command->args2 = &(commandParse->command->data[commandParse->bytes - 1]);
        fprintf(stderr, "Setting args2 to position: %d\n", commandParse->bytes - 1);
      }
      commandParse->prevState = CMD_ARGS;
    }
  }

  if (commandParse->state == CMD_DISPATCHER) {
    if (c == ' ') {
      commandParse->prevState = commandParse->state;
      commandParse->state = CMD_ARGS;
      commandParse->command->data[commandParse->bytes - 1] = '\0';
    } else if (c == '\r') {
      commandParse->state = CMD_CRLF;
      commandParse->command->data[commandParse->bytes - 1] = '\0';
    }
  } else if (commandParse->state == CMD_CRLF) {
    commandParse->state = (c == '\n')
                        ? (commandParse->prevState == CMD_INVALID) ? CMD_DISPATCHER : CMD_OK
                        : CMD_INVALID;
    if (commandParse->state == CMD_DISPATCHER) {
      reset_command_parser(commandParse);
    } else if (commandParse->state == CMD_OK) {
      commandParse->command->data[commandParse->bytes - 2] = '\0';
      commandParse->command->data[commandParse->bytes - 1] = '\0';
    }
    commandParse->prevState = CMD_CRLF;
  } else if (commandParse->state == CMD_ARGS) {
    if (c != ' ' && c != '\t' && c != '\r' && commandParse->prevState == CMD_DISPATCHER) {
      if (commandParse->command->args1 == NULL) {
        commandParse->command->args1 = &(commandParse->command->data[commandParse->bytes - 1]);
      } else {
        commandParse->command->args2 = &(commandParse->command->data[commandParse->bytes - 1]);
      }
      commandParse->prevState = CMD_ARGS;
    } else if (c == ' ' && commandParse->prevState == CMD_ARGS) {
      commandParse->prevState = CMD_DISPATCHER;
      commandParse->command->data[commandParse->bytes - 1] = '\0';
    } else if (c == '\r') {
      commandParse->prevState = CMD_ARGS;
      commandParse->state = CMD_CRLF;
    }
  } else if (commandParse->state == CMD_INVALID) {
    commandParse->prevState = CMD_INVALID;
    commandParse->state = (c == '\r') ? CMD_CRLF : CMD_INVALID;
  }

  return commandParse->state;
}

enum commandState valid_command(buffer * inputBuffer, struct commandParse * commandParse) {
  enum commandState state = commandParse->state;
  if (buffer_can_read(inputBuffer)) {
    state = parse_command(commandParse, buffer_read(inputBuffer));
  }
  return state;
}