#include "account.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WHITESPACE " \t\r\n"

int checkAccountFile(const char *path){
    FILE *file = fopen(path, "r");
    if (file == NULL){
        return -1;
    }
    fclose(file);
    return 0;
}

/* Split an account line "username status" in place. Returns the status
 * (0 or 1) and sets *username, or -1 if the line is malformed. */
static int parseAccountLine(char *line, char **username){
    char *save = NULL;
    char *name = strtok_r(line, WHITESPACE, &save);
    char *status = strtok_r(NULL, WHITESPACE, &save);
    if (name == NULL || status == NULL || strtok_r(NULL, WHITESPACE, &save) != NULL){
        return -1;
    }
    if (strcmp(status, "0") != 0 && strcmp(status, "1") != 0){
        return -1;
    }
    *username = name;
    return status[0] - '0';
}

int findAccount(const char *path, const char *username){
    if (username[0] == '\0'){
        return ACCOUNT_NOT_FOUND;
    }

    FILE *file = fopen(path, "r");
    if (file == NULL){
        return ACCOUNT_FILE_ERROR;
    }

    /* getline grows the buffer as needed: account lines have no length limit. */
    char *line = NULL;
    size_t capacity = 0;
    int result = ACCOUNT_NOT_FOUND;
    while (getline(&line, &capacity, file) != -1){
        char *name = NULL;
        int status = parseAccountLine(line, &name);
        if (status >= 0 && strcmp(name, username) == 0){
            result = (status == 1) ? ACCOUNT_ACTIVE : ACCOUNT_BANNED;
            break;
        }
    }
    if (result == ACCOUNT_NOT_FOUND && ferror(file)){
        result = ACCOUNT_FILE_ERROR;
    }

    free(line);
    fclose(file);
    return result;
}
