#include "protocol.h"
#include <string.h>

int formatSuccessResponse(const StringList *list, char *response, size_t size){
    if (list == NULL || response == NULL || size < 2){
        return -1;
    }

    size_t used = 0;
    response[used++] = SUCCESS_PREFIX;

    for (int i = 0; i < list->count; i++){
        size_t itemLen = strlen(list->items[i]);
        size_t separatorLen = (i > 0) ? 1 : 0;
        if (used + separatorLen + itemLen + 1 > size){
            break;
        }
        if (separatorLen == 1){
            response[used++] = ' ';
        }
        memcpy(response + used, list->items[i], itemLen);
        used += itemLen;
    }

    response[used] = '\0';
    return (int)used;
}

int formatErrorResponse(const char *message, char *response, size_t size){
    if (message == NULL || response == NULL || size < 2){
        return -1;
    }

    size_t messageLen = strlen(message);
    if (messageLen > size - 2){
        messageLen = size - 2;
    }

    response[0] = ERROR_PREFIX;
    memcpy(response + 1, message, messageLen);
    response[1 + messageLen] = '\0';
    return (int)(1 + messageLen);
}
