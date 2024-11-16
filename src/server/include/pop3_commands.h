#ifndef POP3_COMMANDS_H
#define POP3_COMMANDS_H

#include "pop3.h"
#include "commandParse.h"
#include "selector.h"

enum pop3_state executeCommand(struct selector_key * selector_key, struct command * command);

#endif