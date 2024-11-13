#include "pop3.h"

static const char* maildir;
static Connection connections[MAX_CLIENTS] = {0}; 
#define OK_RESPONSE(m) OK_RESP " " m ENTER
#define ERR_RESPONSE(m) ERR_RESP " " m ENTER

// Función para generar una respuesta OK
const char* ok_respon(const char *message) {
    static char response[MAX_POP3_RESPONSE_LENGTH];
    snprintf(response, sizeof(response), "%s %s%s", OK_RESP, message, ENTER);
    return response;
}

// Función para generar una respuesta de error
const char* err_respon(const char *message) {
    static char response[MAX_POP3_RESPONSE_LENGTH];
    snprintf(response, sizeof(response), "%s %s%s", ERR_RESP, message, ENTER);
    return response;
}



// Setea la ruta del directorio de mails, usando un valor por defecto si no se pasa un argumento.
static void set_maildir(const char *dir) {
    maildir = dir ? dir : "./dist/mail";
}

// Crea el directorio de mails si no existe.
static void create_maildir_if_needed() {
    if (access(maildir, F_OK) == -1) {
        mkdir(maildir, S_IRWXU);
    }
}

static  void reset_connections() {
    for (size_t i = 0; i < MAX_CLIENTS; i++) {
        connections[i].username[0] = 0;
        connections[i].authenticated = false;
    }
}

void pop_init(char *dir) {
    set_maildir(dir);
    create_maildir_if_needed();
    reset_connections();
}

int initialize_pop_connection(int sock_fd, struct sockaddr_in client_address)
{
    Connection *client_connection = &connections[sock_fd];
    client_connection->username[0] = '\0';
    client_connection->authenticated = false;

    const char welcome_msg[] = OK_RESPONSE(" POP3 server ready");
    ssize_t sent_bytes = send(sock_fd, welcome_msg, strlen(welcome_msg), 0);
    
    if (sent_bytes < 0)
    {
        LOG("Error sending welcome message to client\n");
        return -1; // Error
    }
    
    return 0; // Success
}


//validacion del nombre de usuario seguro
static bool check_username(const char *username) {
    if (!*username || *username =='.' ){
        return false;
    }
    for (const char *n = username; *n; n++) {
        if (*n == '/' || (n - username) > MAX_USERNAME_LENGTH) {
            return false;
        }
    }
    return true;
}

//verifica la existencia del usuario en el directorio de mails
static bool check_user(const char *username){
    char path[strlen(maildir) + MAX_USERNAME_LENGTH + 1];
    snprintf(path, sizeof(path), "%s/%s", maildir, username);
    return access(path, OK_RESP) != -1;
}

//verificamos la contraseña del usuario
static bool valid_password(const char *username, const char *pass) {
    char path[strlen(maildir) + MAX_USERNAME_LENGTH + sizeof("/data/pass")];
    snprintf(path, sizeof(path), "%s/%s/data/pass", maildir, username);
    FILE *file = fopen(path, "r");
    if (!file) {
        return false;
    }
    char buffer[MAX_PASSWORD_LENGTH + 1];
    fgets(buffer, sizeof(buffer), file);
    fclose(file);
    return strcmp(buffer, pass) == 0;
}

//manejo del comando USER
static size_t handle_user(Connection *client, const char *username, char **response){
    if (check_username(username) || !check_user(username)){
        *response = ERR_RESPONSE(" Invalid username");
        return strlen(*response);
    }
}

//manejo del comando PASS
static size_t handle_pass(Connection *client, const char *pass, char **response){
    if (!valid_password(client->username, pass)){
        client->username[0] = '\0';
        *response = ERR_RESPONSE(" Invalid password");
        return strlen(*response);
    }
    client->authenticated = true;
    *response = OK_RESPONSE("Authentication successful");
    return strlen(*response);
}
// Manejo del comando STAT
static size_t handle_stat(Connection *client, char **response) {
    size_t size = 0, count = 0;

    for (Mailfile *mail = client->mails; mail->uid[0]; mail++) {
        if (!mail->deleted) {
            size += mail->size;
            count++;
        }
    }

    char buffer[MAX_POP3_RESPONSE_LENGTH];
    snprintf(buffer, sizeof(buffer), OK_RESPONSE("%zu %zu"), count, size);
    *response = strdup(buffer);
    return strlen(*response);
}

// Manejo de comandos
size_t process_command(int client_fd, const char *command) {
    Connection *client = &connections[client_fd];
    char cmd[BUFFER_SIZE], arg[BUFFER_SIZE];
    sscanf(command, "%s %s", cmd, arg);

    char *response = NULL;
    size_t response_len = 0;

    if (strcasecmp(cmd, "USER") == 0) {
        response_len = handle_user(client, arg, &response);
    } else if (strcasecmp(cmd, "PASS") == 0) {
        response_len = handle_pass(client, arg, &response);
    } else if (strcasecmp(cmd, "STAT") == 0 && client->authenticated) {
        response_len = handle_stat(client, &response);
    } else if (strcasecmp(cmd, "QUIT") == 0) {
        ok_respon(response, sizeof(response), "bai");
        response_len = strlen(response);
        send(client_fd, response, response_len, 0);
        return CLOSE_CONNECTION;
    } else {
        err_respon(response, sizeof(response), "Invalid command");
        response_len = strlen(response);
    }

    send(client_fd, response, response_len, 0);
    if (response) free(response);

    return KEEP_CONNECTION_OPEN;
}