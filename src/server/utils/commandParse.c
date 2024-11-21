#include "commandParse.h"
#include <stdlib.h>

struct commandParse * commandParseInit() {
  struct commandParse * commandParse = malloc(sizeof(struct commandParse));
  commandParse->state = commandParse->prevState = CMD_DISPATCHER;
  commandParse->command = 0;
  commandParse->command = malloc(sizeof(struct command));
  commandParse->command->args1 = NULL;
  commandParse->command->args2 = NULL;

  return commandParse;
}

void reset_commandParser(struct commandParse * command_parse) {
  command_parse->state = command_parse->prevState = CMD_DISPATCHER;
  command_parse->bytes = 0;
  command_parse->command->args1 = command_parse->command->args2 = NULL;
}

static enum commandState parse_command(struct commandParse * commandParse, uint8_t c) {
  commandParse->command->data[commandParse->bytes] = c;
  commandParse->bytes++;
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
      // TODO: reset parser;
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