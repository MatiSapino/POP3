#include <string.h>
#include <strings.h>
#include "pop3_commands.h"
#include "user.h"


extern struct pop3_args pop3_args;  //ver que onda este extern

struct command_function {
    char * name;
    enum pop3_state (*function)(struct selector_key * selector_key, struct command * command);
};

static enum pop3_state executeUSER(struct selector_key *selector_key, struct command *command);
static enum pop3_state executePASS(struct selector_key *selector_key, struct command *command);
static enum pop3_state executeQUIT(struct selector_key *selector_key, struct command *command);
static enum pop3_state executeCAPA(struct selector_key *selector_key, struct command *command);
static enum pop3_state executeSTAT(struct selector_key *selector_key, struct command *command);
static enum pop3_state executeLIST(struct selector_key *selector_key, struct command *command);
static enum pop3_state executeRETR(struct selector_key *selector_key, struct command *command);
static enum pop3_state executeDELE(struct selector_key *selector_key, struct command *command);
static enum pop3_state executeNOOP(struct selector_key *selector_key, struct command *command);
static enum pop3_state executeRSET(struct selector_key *selector_key, struct command *command);

static void listCommands(struct command_function commands[], struct Client *client);

static struct command_function commands_anonymous[] = {
    {"USER", executeUSER},
    {"PASS", executePASS},
    {"QUIT", executeQUIT},
    {"CAPA", executeCAPA},
    {NULL, NULL},
};

static struct command_function commands_authenticated[] = {
    {"STAT", executeSTAT},
    {"LIST", executeLIST},
    {"RETR", executeRETR},
    {"DELE", executeDELE},
    {"NOOP", executeNOOP},
    {"RSET", executeRSET},   
    {"QUIT", executeQUIT},
    {"CAPA", executeCAPA},
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

    if(client->authenticated) commandFunction = commands_authenticated;
    else commandFunction = commands_anonymous;

    for (i = 0; commandFunction[i].name != NULL; i++) {
        if (strcasecmp(commandFunction[i].name, (char *)command->data) == 0) {
            return commandFunction[i].function(selector_key, command);
        }
    }

    return STATE_WRITE;
}


static enum pop3_state executeUSER(struct selector_key * selector_key, struct command * command){
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

static enum pop3_state executePASS(struct selector_key * key, struct command * command){
    fprintf(stderr, "Executing PASS\n");
    struct Client * client = key->data;
    if (command->args1 == NULL) {
        errResponse(client, "Invalid arguments");
        return STATE_WRITE;
    }

    if(check_password(client->user->username, (char*)command->args1, client)){
        strcpy(client->user->password, (char*)command->args1);
        client->authenticated = true;
        okResponse(client, "maildrop locked and ready");
    }     
    return STATE_WRITE;   
}

static enum pop3_state executeQUIT(struct selector_key *key, struct command *command){
    struct Client * client = key->data;
    client->closed = true; 
    okResponse(client, "Bye");
    return STATE_CLOSE;
}

static enum pop3_state executeCAPA(struct selector_key *key, struct command *command){
    struct Client * client = key->data;
    fprintf(stderr, "Executing CAPA\n");
    okResponse(client, "Capability list follows");

    if (client->authenticated) listCommands(commands_authenticated, client);
    else listCommands(commands_anonymous, client);

    response(client, ".");
    return STATE_WRITE;
}

static void listCommands(struct command_function commands[], struct Client* client) {
    for (int i = 0; commands[i].name != NULL; i++) {
        response(client, commands[i].name);
    }
}

static enum pop3_state executeSTAT(struct selector_key *key, struct command *command) {
    struct Client *client = key->data;
    
    fprintf(stderr, "DEBUG: Iniciando comando STAT\n");
    
    if (!client->authenticated) {
        fprintf(stderr, "DEBUG: Cliente no autenticado\n");
        errResponse(client, "not authenticated");
        return STATE_WRITE;
    }
    
    fprintf(stderr, "DEBUG: Cliente autenticado como: %s\n", client->user->username);
    
    if (client->user == NULL) {
        fprintf(stderr, "DEBUG: Error: client->user es NULL\n");
        errResponse(client, "internal error");
        return STATE_WRITE;
    }
    
    struct mailbox *box = get_user_mailbox(client->user->username);
    if (!box) {
        fprintf(stderr, "DEBUG: Error obteniendo mailbox\n");
        errResponse(client, "mailbox error");
        return STATE_WRITE;
    }
    
    char response[100];
    snprintf(response, sizeof(response), "%zu %zu", 
             box->mail_count, box->total_size);
             
    fprintf(stderr, "DEBUG: Enviando respuesta: %s\n", response);
    okResponse(client, response);
    
    free(box);
    return STATE_WRITE;
}

static enum pop3_state executeLIST(struct selector_key *key, struct command *command){
    struct Client * client = key->data;
    fprintf(stderr, "Executing LIST\n");
    return STATE_WRITE;
}

static enum pop3_state executeRETR(struct selector_key *key, struct command *command){
    struct Client * client = key->data;
    fprintf(stderr, "Executing RETR\n");
    return STATE_WRITE;
}

static enum pop3_state executeDELE(struct selector_key *key, struct command *command){
    struct Client * client = key->data;
    fprintf(stderr, "Executing DELE\n");
    return STATE_WRITE;
}

static enum pop3_state executeNOOP(struct selector_key *key, struct command *command){
    struct Client * client = key->data;
    fprintf(stderr, "Executing NOOP\n");
    return STATE_WRITE;
}

static enum pop3_state executeRSET(struct selector_key *key, struct command *command){
    struct Client * client = key->data;
    fprintf(stderr, "Executing RSET\n");
    return STATE_WRITE;
}

