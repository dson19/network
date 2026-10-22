#include "protocol.h"
#include <string.h>

/* If `line` is `keyword` alone or `keyword` + ' ' + argument, set *argument
 * to the text after the space ("" if none) and return 1; otherwise return 0. */
static int matchCommand(const char *line, const char *keyword, const char **argument){
    size_t keywordLen = strlen(keyword);
    if (strncmp(line, keyword, keywordLen) != 0){
        return 0;
    }
    if (line[keywordLen] == '\0'){
        *argument = line + keywordLen;
        return 1;
    }
    if (line[keywordLen] == ' '){
        *argument = line + keywordLen + 1;
        return 1;
    }
    return 0;
}

int parseRequest(const char *line, const char **argument){
    if (matchCommand(line, LOGIN_COMMAND, argument)){
        return REQUEST_LOGIN;
    }
    if (matchCommand(line, POST_COMMAND, argument)){
        return REQUEST_POST;
    }
    if (strcmp(line, LOGOUT_COMMAND) == 0){
        return REQUEST_LOGOUT;
    }
    return REQUEST_UNKNOWN;
}
