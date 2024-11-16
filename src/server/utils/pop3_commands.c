#include <string.h>
#include <strings.h>
#include "pop3_commands.h"


extern struct pop3_args pop3_args;  //ver que onda este extern

struct command_function {
    char * name;
    enum pop3_state (*function)(struct selector_key * selector_key, struct command * command);
};

static enum pop3_state executeUser(struct selector_key *key, struct command *command);

static struct command_function commands[] = {
    {"USER", executeUser},
    {NULL, NULL},
};

enum pop3_state executeCommand(struct selector_key * selector_key, struct command * command) {
    struct Client * client = selector_key->data;
    struct command_function * commandFunction;
    int i;

    commandFunction = commands;

    for (i = 0; commandFunction[i].name != NULL; i++) {
        if (strcasecmp(commandFunction[i].name, (char *)command->data) == 0) {
            return commandFunction[i].function(selector_key, command);
        }
    }

    return STATE_WRITE;
}


static enum pop3_state executeUser(struct selector_key * selector_key, struct command * command){
    struct Client * client = selector_key->data;
    strncpy(client->user->username, (char *)command->args1, 41);
    okResponse(client, "User accepted");
    return STATE_WRITE;
}