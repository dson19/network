#include "util.h"
#include <stdio.h>
#include <string.h>

int readLine(char *buffer, size_t size){
    if (fgets(buffer, (int)size, stdin) == NULL){
        return READ_EOF;
    }

    size_t length = strlen(buffer);
    int tooLong = 0;
    if (length > 0 && buffer[length - 1] == '\n'){
        buffer[--length] = '\0';
    } else {
        /* No line break: either the line is too long or it is the last line. */
        int c;
        while ((c = getchar()) != EOF && c != '\n'){
            tooLong = 1;
        }
    }

    while (length > 0 && buffer[length - 1] == '\r'){
        buffer[--length] = '\0';
    }
    return tooLong ? READ_TOO_LONG : READ_OK;
}

const char *getBaseName(const char *path){
    const char *slash = strrchr(path, '/');
    return (slash == NULL) ? path : slash + 1;
}

void printResponse(const char *response){
    if (response[0] == '+' || response[0] == '-'){
        printf("Server: %s\n", response);
    } else {
        printf("Invalid response from server\n");
    }
}
