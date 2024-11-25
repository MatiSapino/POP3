#include <string.h>
#include <strings.h>
#include "pop3_commands.h"
#include "user.h"


extern struct pop3_args pop3_args;  //ver que onda este extern

struct command_function {
    char * name;
    enum pop3_state (*function)(struct selector_key * selector_key, struct command * command);
};

static enum pop3_state executeUser(struct selector_key *key, struct command *command);
static enum pop3_state executePass(struct selector_key *key, struct command *command);
static enum pop3_state executeQuit(struct selector_key *key, struct command *command);
static enum pop3_state executeCapa(struct selector_key *key, struct command *command);


static struct command_function commands_anonymous[] = {
    {"USER", executeUser},
    {"PASS", executePass},
    {"QUIT", executeQuit},
    {"CAPA", executeCapa},
    {NULL, NULL},
};

static struct command_function commands_authenticated[] = {
    {"QUIT", executeQuit},
    {NULL, NULL},
};

enum pop3_state executeCommand(struct selector_key * selector_key, struct command * command) {
    fprintf(stderr, "Executing command\n");
    struct Client * client = selector_key->data;
    struct command_function * commandFunction;
    int i;

    // Verificar si selector_key o client son NULL
    if (selector_key == NULL) {
        fprintf(stderr, "selector_key is NULL in executeCommand\n");
        return STATE_ERROR;
    }
    if (client == NULL) {
        fprintf(stderr, "client is NULL in executeCommand\n");
        return STATE_ERROR;
    }
    if (client->user == NULL) {
        fprintf(stderr, "client->user is NULL in executeCommand\n");
    }
    
    fprintf(stderr, "Executing command: %s\n", (char *)command->data);

    commandFunction = commands_anonymous;

    for (i = 0; commandFunction[i].name != NULL; i++) {
        if (strcasecmp(commandFunction[i].name, (char *)command->data) == 0) {
            return commandFunction[i].function(selector_key, command);
        }
    }

    return STATE_WRITE;
}


static enum pop3_state executeUser(struct selector_key * selector_key, struct command * command){
    struct Client * client = selector_key->data;
    if(client->authenticated){
        errResponse(client, "Already authenticated");
        return STATE_ERROR;
    }

    if(client->user == NULL){
        fprintf(stderr, "Client->user is NULL\n");
        errResponse(client, "Internal server error");
        return STATE_WRITE;
    }

    if(command->args1 == NULL){
        return STATE_WRITE;
    }
    if(check_user((char *)command->args1, client)){
        strncpy(client->user->username, (char *)command->args1, MAX_USERNAME-1);
        client->user->username[MAX_USERNAME - 1] = '\0';
    }
    return STATE_WRITE;
}

static enum pop3_state executePass(struct selector_key * key, struct command * command){
    fprintf(stderr, "Executing PASS\n");
    struct Client * client = key->data;
    if (command->args1 == NULL) {
        errResponse(client, "Invalid arguments");
        return STATE_WRITE;
    }

    if(check_password(client->user->username, (char*)command->args1, client)){
        strcpy(client->user->password, (char*)command->args1);
        okResponse(client, "maildrop locked and ready");
    }     
    return STATE_WRITE;   
}

static enum pop3_state executeQuit(struct selector_key *key, struct command *command){
    struct Client * client = key->data;
    client->closed = true; 
    return STATE_WRITE;
}

static enum pop3_state executeCapa(struct selector_key *key, struct command *command){
    struct Client * client = key->data;
    fprintf(stderr, "Executing CAPA\n");
    okResponse(client, "Capability list follows");
    int commands_count = sizeof(commands_authenticated)/sizeof(commands_authenticated[0]);
    for (size_t i = 0; i < commands_count; i++)
    {
        okResponse(client, commands_authenticated[i].name);
    }
    okResponse(client, ".");
    return STATE_WRITE;
}