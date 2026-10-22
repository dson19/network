#include "protocol.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

bool isValidFileName(const char *name){
    if (name == NULL){
        return false;
    }

    size_t length = strlen(name);
    if (length == 0 || length > MAX_FILE_NAME_LEN){
        return false;
    }
    if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0){
        return false;
    }
    if (name[0] == ' ' || name[length - 1] == ' '){
        return false;
    }

    for (size_t i = 0; i < length; i++){
        unsigned char c = (unsigned char)name[i];
        if (c == '/' || c < 0x20 || c == 0x7f){
            return false;
        }
    }
    return true;
}

/* Parse a file size made of decimal digits only. Returns a PARSE_* code.
 * strtoull alone is not enough: it accepts signs and spaces, e.g. "-1". */
static int parseFileSize(const char *text, uint64_t *fileSize){
    if (*text == '\0'){
        return PARSE_INVALID_SIZE;
    }
    for (const char *p = text; *p != '\0'; p++){
        if (!isdigit((unsigned char)*p)){
            return PARSE_INVALID_SIZE;
        }
    }

    errno = 0;
    unsigned long long value = strtoull(text, NULL, 10);
    if (errno == ERANGE || value > MAX_FILE_SIZE){
        return PARSE_TOO_LARGE;
    }
    *fileSize = (uint64_t)value;
    return PARSE_OK;
}

int parseUploadCommand(const char *line, char *fileName, size_t nameSize, uint64_t *fileSize){
    size_t keywordLen = strlen(UPLOAD_COMMAND);
    if (strncmp(line, UPLOAD_COMMAND, keywordLen) != 0 || line[keywordLen] != ' '){
        return PARSE_INVALID_COMMAND;
    }

    const char *name = line + keywordLen + 1;
    const char *lastSpace = strrchr(name, ' ');
    if (lastSpace == NULL){
        return PARSE_INVALID_COMMAND;
    }

    size_t nameLen = (size_t)(lastSpace - name);
    if (nameLen == 0 || nameLen > MAX_FILE_NAME_LEN || nameLen >= nameSize){
        return PARSE_INVALID_NAME;
    }
    memcpy(fileName, name, nameLen);
    fileName[nameLen] = '\0';
    if (!isValidFileName(fileName)){
        return PARSE_INVALID_NAME;
    }

    return parseFileSize(lastSpace + 1, fileSize);
}

const char *parseErrorReply(int status){
    switch (status){
        case PARSE_INVALID_NAME:
            return REPLY_INVALID_NAME;
        case PARSE_INVALID_SIZE:
            return REPLY_INVALID_SIZE;
        case PARSE_TOO_LARGE:
            return REPLY_FILE_TOO_LARGE;
        default:
            return REPLY_INVALID_COMMAND;
    }
}
