#ifndef COMMANDPARSE_H
#define COMMANDPARSE_H

#include <stdint.h>
#include "buffer.h"

#define CMD_MAX_LENGHT 100

struct command {
  uint8_t data[CMD_MAX_LENGHT];
  uint8_t * args1;
  uint8_t * args2;
};

enum commandState {
  CMD_DISPATCHER,
  CMD_ARGS,
  CMD_CRLF,
  CMD_OK,
  CMD_INVALID,
};

struct commandParse {
    enum commandState state;
    enum commandState prevState;
    struct command * command;
    int bytes;
};

struct commandParse * commandParseInit();

void reset_commandParser(struct commandParse * command_parse);

enum commandState valid_command(buffer * inputBuffer, struct commandParse * commandParse);

#endif //COMMANDPARSE_H