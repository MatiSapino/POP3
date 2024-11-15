/**
 *  stm.c - pequeño motor de maquina de estados donde los eventos son los
 *         del selector.c
 */
#include <stdlib.h>
#include "stm.h"

#define N(x) (sizeof(x)/sizeof((x)[0]))

struct selector_key *key;

void stm_init(struct state_machine *stm) {
    // verificamos que los estados son correlativos, y que están bien asignados.
    for(unsigned i = 0 ; i <= stm->max_state; i++) {
        if(i != stm->states[i].state) {
            abort();
        }
    }

    if(stm->initial < stm->max_state) {
        stm->current = NULL;
    } else {
        abort();
    }
}

inline static void handle_first(struct state_machine *stm, struct selector_key *key) {
    if(stm->current == NULL) {
        stm->current = stm->states + stm->initial;
        if(NULL != stm->current->on_arrival) {
            stm->current->on_arrival(stm->current->state, key);
        }
    }
}

inline static void jump(struct state_machine *stm, unsigned next, struct selector_key *key) {
    if(next > stm->max_state) {
        abort();
    }
    if(stm->current != stm->states + next) {
        if(stm->current != NULL && stm->current->on_departure != NULL) {
            stm->current->on_departure(stm->current->state, key);
        }
        stm->current = stm->states + next;

        if(NULL != stm->current->on_arrival) {
            stm->current->on_arrival(stm->current->state, key);
        }
    }
}

unsigned stm_handler_read(struct state_machine *stm, struct selector_key *key) {
    handle_first(stm, key);
    if(stm->current->on_read_ready == 0) {
        abort();
    }
    const unsigned int ret = stm->current->on_read_ready(key);
    jump(stm, ret, key);

    return ret;
}

unsigned stm_handler_write(struct state_machine *stm, struct selector_key *key) {
    handle_first(stm, key);
    if(stm->current->on_write_ready == 0) {
        abort();
    }
    const unsigned int ret = stm->current->on_write_ready(key);
    jump(stm, ret, key);

    return ret;
}

unsigned stm_handler_block(struct state_machine *stm, struct selector_key *key) {
    handle_first(stm, key);
    if(stm->current->on_block_ready == 0) {
        abort();
    }
    const unsigned int ret = stm->current->on_block_ready(key);
    jump(stm, ret, key);

    return ret;
}

void stm_handler_close(struct state_machine *stm, struct selector_key *key) {
    if(stm->current != NULL && stm->current->on_departure != NULL) {
        stm->current->on_departure(stm->current->state, key);
    }
}

unsigned stm_state(struct state_machine *stm) {
    unsigned ret = stm->initial;
    if(stm->current != NULL) {
        ret= stm->current->state;
    }
    return ret;
}

void stm_parse(char * buffer, struct selector_key *key, Client * client){

    if (buffer == NULL) {
        fprintf(stderr, "Received NULL data in stm_parse\n");
        return;
    }

    if (strcmp(buffer, "USER") == 0) {
        fprintf(stderr, "User command %s\n, buffer");
        jump(client->stm, STATE_WAIT_USERNAME, key);
    }
    // else if (user->stm->current->state == STATE_WAIT_USERNAME) {
    //     bool valid = check_user(buffer, "maildir");

    //     if (valid) {
    //         jump(user->stm, STATE_WAIT_PASS, key);
    //     } else {
    //         fprintf(stderr, "User not valid\n");
    //         return;
    //     }
    // }
    // else if (user->stm->current->state == STATE_WAIT_PASS) {
    //     if (strcmp(buffer, "PASS") == 0) {
    //         jump(user->stm, STATE_WAIT_PASSWORD, key);
    //     }
    // }
    // else if (user->stm->current->state == STATE_WAIT_PASSWORD) {
    //     bool valid = check_password("username", buffer, "maildir");

    //     if (valid) {
    //         jump(user->stm, STATE_AUTHENTICATED, key);
    //     } else {
    //         fprintf(stderr, "Password not valid\n");
    //         return;
    //     }
    // }
    // else if (user->stm->current->state == STATE_AUTHENTICATED) {
    //     fprintf(stderr, "Authenticated\n");
    //     if (strcmp(buffer, "STAT") == 0) {
    //         handle_stat(user);
    //     }
    //     else if (strcmp(buffer, "LIST") == 0) {
    //         handle_list(user);
    //     }
    //     else if(strcmp(buffer, "QUIT")){
    //         handle_quit(user);
    //     }
    //     else if (strcmp(buffer, "DELE")==0){
    //         jump(user->stm, STATE_TO_DELE, key);
    //     }
    //     else if (strcmp(buffer, "RETR") == 0) {
    //         jump(user->stm, STATE_TO_RETR, key);
    //     }
    // }
    // else if (user->stm->current->state == STATE_TO_DELE) {
    //     handle_dele(user, buffer);
    //     jump(user->stm, STATE_AUTHENTICATED, key);
    // }
    // else if (user->stm->current->state == STATE_TO_RETR) {
    //     handle_retr(user, buffer);
    //     jump(user->stm, STATE_AUTHENTICATED, key);
    // }
    else {
        fprintf(stderr, "Unknown command: %s\n", buffer);
            return;
    }
    
}