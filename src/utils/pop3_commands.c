#include <string.h>
#include <strings.h>
#include "pop3_commands.h"
#include "user.h"
#include "../include/transform.h"
#include "../include/metrics.h"


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
        init_mailbox(client->user->username, client);
        okResponse(client, "maildrop locked and ready");
    }     
    return STATE_WRITE;   
}

static enum pop3_state executeQUIT(struct selector_key *key, struct command *command) {
    struct Client *client = key->data;
    
    fprintf(stderr, "DEBUG: Executing QUIT command\n");
    
    if (client->authenticated) {
        // Obtener el mailbox del usuario
        struct mailbox *box = get_user_mailbox(client->user->username);
        if (box) {
            // Eliminar físicamente los mensajes marcados
            char filepath[PATH_MAX];
            char maildir[PATH_MAX];
            
            snprintf(maildir, sizeof(maildir), "%s/maildir/%s/new", 
                     getenv("HOME"), client->user->username);
            
            for (size_t i = 0; i < box->mail_count; i++) {
                if (box->mails[i].deleted) {
                    snprintf(filepath, sizeof(filepath), "%s/%s", 
                             maildir, box->mails[i].filename);
                    unlink(filepath);  // Eliminar el archivo
                }
            }
            
            free_mailbox(box);
        }
    }
    
    client->closed = true;
    okResponse(client, "POP3 server signing off");
    
    log_command(client->authenticated ? client->user->username : "anonymous", 
                "QUIT", "session ended");
    
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

static enum pop3_state executeLIST(struct selector_key *key, struct command *command) {
    struct Client *client = key->data;
    
    fprintf(stderr, "DEBUG: Executing LIST command\n");

    // Verificar autenticación
    if (!client->authenticated) {
        fprintf(stderr, "DEBUG: Client not authenticated\n");
        errResponse(client, "not authenticated");
        return STATE_WRITE;
    }
    
    // Obtener el mailbox del usuario
    struct mailbox *box = get_user_mailbox(client->user->username);
    if (!box) {
        fprintf(stderr, "DEBUG: Mailbox error for user: %s\n", client->user->username);
        errResponse(client, "mailbox error");
        return STATE_WRITE;
    }

    // Si se proporciona un número de mensaje específico
    if (command->args1 != NULL) {
        char *endptr;
        long msg_num = strtol((char *)command->args1, &endptr, 10);
        
        if (*endptr != '\0' || msg_num <= 0 || msg_num > box->mail_count) {
            fprintf(stderr, "DEBUG: Invalid message number: %s\n", (char *)command->args1);
            errResponse(client, "no such message");
            free(box);
            return STATE_WRITE;
        }

        struct mail *mail = &box->mails[msg_num - 1];
        if (mail->deleted) {
            errResponse(client, "message is deleted");
            free(box);
            return STATE_WRITE;
        }

        char response[64];
        snprintf(response, sizeof(response), "%ld %zu", msg_num, mail->size);
        okResponse(client, response);
    } 
    // Listar todos los mensajes
    else {
        char res[64];
        size_t valid_messages = 0;
        size_t total_size = 0;

        // Contar mensajes válidos y tamaño total
        for (size_t i = 0; i < box->mail_count; i++) {
            if (!box->mails[i].deleted) {
                valid_messages++;
                total_size += box->mails[i].size;
            }
        }

        snprintf(res, sizeof(res), "%zu messages (%zu octets)", 
                valid_messages, total_size);
        okResponse(client, res);

        // Listar cada mensaje no eliminado
        for (size_t i = 0; i < box->mail_count; i++) {
            if (!box->mails[i].deleted) {
                snprintf(res, sizeof(res), "%zu %zu", 
                        i + 1, box->mails[i].size);
                response(client, res);
            }
        }
        response(client, ".");
    }

    // Registrar el comando en el log
    log_command(client->user->username, "LIST", 
                command->args1 ? (char *)command->args1 : "all messages");

    free(box);
    return STATE_WRITE;
}

static enum pop3_state executeRETR(struct selector_key *key, struct command *command) {
    struct Client *client = key->data;
    
    fprintf(stderr, "Executing RETR\n");

    if (!client->authenticated) {
        fprintf(stderr, "Client not authenticated\n");
        errResponse(client, "not authenticated");
        return STATE_WRITE;
    }
    
    if (command->args1 == NULL) {
        fprintf(stderr, "Missing message number\n");
        errResponse(client, "missing message number");
        return STATE_WRITE;
    }
    
    char *endptr;
    long msg_num = strtol((char *)command->args1, &endptr, 10);
    if (*endptr != '\0' || msg_num <= 0) {
        fprintf(stderr, "Invalid message number: %s\n", (char *)command->args1);
        errResponse(client, "invalid message number");
        return STATE_WRITE;
    }
    
    struct mailbox *box = get_user_mailbox(client->user->username);
    if (!box) {
        fprintf(stderr, "Mailbox error for user: %s\n", client->user->username);
        errResponse(client, "mailbox error");
        return STATE_WRITE;
    }
    
    fprintf(stderr, "Mailbox obtained for user: %s\n", client->user->username);

    if (msg_num > box->mail_count) {
        fprintf(stderr, "No such message: %ld\n", msg_num);
        errResponse(client, "no such message");
        free(box);
        return STATE_WRITE;
    }
    
    struct mail *mail = &box->mails[msg_num - 1];
    fprintf(stderr, "Message found: %ld\n", msg_num);
    fprintf(stderr, "Message filename: %s\n", mail->filename);
    fprintf(stderr, "Message size: %ld\n", mail->size);

    if (mail->deleted) {
        fprintf(stderr, "Message has been deleted: %ld\n", msg_num);
        errResponse(client, "message has been deleted");
        free(box);
        return STATE_WRITE;
    }
    
    char input_path[PATH_MAX];
    char output_path[PATH_MAX];
    char maildir[PATH_MAX];
    
    if (getenv("HOME") == NULL) {
        fprintf(stderr, "HOME environment variable not set\n");
        errResponse(client, "internal server error");
        free(box);
        return STATE_WRITE;
    }
    
    snprintf(maildir, sizeof(maildir), "%s/maildir/%s/new", 
             getenv("HOME"), client->user->username);
    
    snprintf(input_path, sizeof(input_path), "%s/%s", 
             maildir, mail->filename);
    
    snprintf(output_path, sizeof(output_path), "%s/%s.transformed", 
             maildir, mail->filename);
    
    fprintf(stderr, "Input path: %s\n", input_path);
    // fprintf(stderr, "Output path: %s\n", output_path);

    // if (!transform_apply(input_path, output_path)) {
    //     fprintf(stderr, "Transformation error for file: %s\n", input_path);
    //     errResponse(client, "transformation error");
    //     free(box);
    //     return STATE_WRITE;
    // }
    
    // fprintf(stderr, "Transformation applied to file: %s\n", input_path);

    // FILE *file = fopen(output_path, "r");
    FILE *file = fopen(input_path, "r");
    if (!file) {
        fprintf(stderr, "Could not open transformed message: %s\n", output_path);
        errResponse(client, "could not open message");
        unlink(output_path);
        free(box);
        return STATE_WRITE;
    }
    
    fseek(file, 0, SEEK_END);
    long transformed_size = ftell(file);
    fseek(file, 0, SEEK_SET);
    
    fprintf(stderr, "fseek terminado\n");

    char res[64];
    snprintf(res, sizeof(res), "message follows (%ld octets)", transformed_size);
    okResponse(client, res);
    
    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        size_t limit;
        uint8_t *write_ptr = buffer_write_ptr(&client->outputBuffer, &limit);
        size_t to_write = bytes_read < limit ? bytes_read : limit;
        
        memcpy(write_ptr, buffer, to_write);
        buffer_write_adv(&client->outputBuffer, to_write);
    }
    
    response(client, "\r\n.\r\n");
    
    fclose(file);
    unlink(output_path);
    free(box);
    
    return STATE_WRITE;
}

static enum pop3_state executeDELE(struct selector_key *key, struct command *command) {
    struct Client *client = key->data;
    
    fprintf(stderr, "DEBUG: Executing DELE command\n");

    // Verificar autenticación
    if (!client->authenticated) {
        fprintf(stderr, "DEBUG: Client not authenticated\n");
        errResponse(client, "not authenticated");
        return STATE_WRITE;
    }
    
    // Verificar que se proporcionó un número de mensaje
    if (command->args1 == NULL) {
        fprintf(stderr, "DEBUG: Missing message number\n");
        errResponse(client, "missing message number");
        return STATE_WRITE;
    }
    
    // Convertir el argumento a número
    char *endptr;
    long msg_num = strtol((char *)command->args1, &endptr, 10);
    if (*endptr != '\0' || msg_num <= 0) {
        fprintf(stderr, "DEBUG: Invalid message number: %s\n", (char *)command->args1);
        errResponse(client, "invalid message number");
        return STATE_WRITE;
    }
    
    // Obtener el mailbox del usuario
    struct mailbox *box = get_user_mailbox(client->user->username);
    if (!box) {
        fprintf(stderr, "DEBUG: Mailbox error for user: %s\n", client->user->username);
        errResponse(client, "mailbox error");
        return STATE_WRITE;
    }
    
    // Verificar que el número de mensaje es válido
    if (msg_num > box->mail_count) {
        fprintf(stderr, "DEBUG: No such message: %ld\n", msg_num);
        errResponse(client, "no such message");
        free(box);
        return STATE_WRITE;
    }
    
    // Verificar si el mensaje ya está marcado como eliminado
    struct mail *mail = &box->mails[msg_num - 1];
    if (mail->deleted) {
        fprintf(stderr, "DEBUG: Message already deleted: %ld\n", msg_num);
        errResponse(client, "message already deleted");
        free(box);
        return STATE_WRITE;
    }
    
    // Marcar el mensaje como eliminado
    mail->deleted = true;
    box->total_size -= mail->size;  // Actualizar el tamaño total del mailbox
    
    // Registrar la acción en el log
    char response[64];
    snprintf(response, sizeof(response), "message %ld deleted", msg_num);
    log_command(client->user->username, "DELE", response);
    
    // Enviar respuesta exitosa
    okResponse(client, response);
    
    free(box);
    return STATE_WRITE;
}

static enum pop3_state executeNOOP(struct selector_key *key, struct command *command) {
    struct Client *client = key->data;
    
    fprintf(stderr, "DEBUG: Executing NOOP command\n");

    // Verificar autenticación
    if (!client->authenticated) {
        fprintf(stderr, "DEBUG: Client not authenticated\n");
        errResponse(client, "not authenticated");
        return STATE_WRITE;
    }
    
    // Registrar el comando en el log
    log_command(client->user->username, "NOOP", "OK");
    
    // Enviar respuesta exitosa
    okResponse(client, "done");
    
    return STATE_WRITE;
}

static enum pop3_state executeRSET(struct selector_key *key, struct command *command) {
    struct Client *client = key->data;
    
    fprintf(stderr, "DEBUG: Executing RSET command\n");

    // Verificar autenticación
    if (!client->authenticated) {
        fprintf(stderr, "DEBUG: Client not authenticated\n");
        errResponse(client, "not authenticated");
        return STATE_WRITE;
    }
    
    // Obtener el mailbox del usuario
    struct mailbox *box = get_user_mailbox(client->user->username);
    if (!box) {
        fprintf(stderr, "DEBUG: Mailbox error for user: %s\n", client->user->username);
        errResponse(client, "mailbox error");
        return STATE_WRITE;
    }
    
    // Restaurar todos los mensajes marcados como eliminados
    size_t restored_count = 0;
    box->total_size = 0;  // Recalcular el tamaño total
    
    for (size_t i = 0; i < box->mail_count; i++) {
        if (box->mails[i].deleted) {
            box->mails[i].deleted = false;
            restored_count++;
        }
        box->total_size += box->mails[i].size;
    }
    
    // Registrar la acción en el log
    char res[64];
    snprintf(res, sizeof(res), "%zu messages restored", restored_count);
    log_command(client->user->username, "RSET", res);
    
    // Enviar respuesta exitosa
    okResponse(client, "maildrop has been reset");
    
    free(box);
    return STATE_WRITE;
}

