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
