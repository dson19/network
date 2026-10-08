#include "validator.h"
#include <arpa/inet.h>
#include <ctype.h>
#include <string.h>

bool isValidIPv4(const char *text){
    if (text == NULL){
        return false;
    }

    /* inet_pton is strict: it rejects "1.2.3", "256.1.1.1", "01.2.3.4" and
     * trailing garbage, unlike inet_aton. */
    unsigned char buffer[sizeof(struct in_addr)];
    return inet_pton(AF_INET, text, buffer) == 1;
}

/* Check one label [start, start + len): 1-63 characters, letters/digits/'-',
 * not starting or ending with '-'. */
static bool isValidLabel(const char *start, size_t len){
    if (len == 0 || len > MAX_LABEL_LEN){
        return false;
    }
    if (start[0] == '-' || start[len - 1] == '-'){
        return false;
    }
    for (size_t i = 0; i < len; i++){
        unsigned char c = (unsigned char)start[i];
        if (!isalnum(c) && c != '-'){
            return false;
        }
    }
    return true;
}

/* Return true if every character of [start, start + len) is a digit. */
static bool isAllDigits(const char *start, size_t len){
    for (size_t i = 0; i < len; i++){
        if (!isdigit((unsigned char)start[i])){
            return false;
        }
    }
    return true;
}

bool isValidDomain(const char *text){
    if (text == NULL){
        return false;
    }

    size_t len = strlen(text);
    if (len > 0 && text[len - 1] == '.'){
        len--; /* a single trailing dot marks a fully qualified name */
    }
    if (len == 0 || len > MAX_DOMAIN_LEN){
        return false;
    }

    size_t labelStart = 0;
    for (size_t i = 0; i <= len; i++){
        if (i == len || text[i] == '.'){
            size_t labelLen = i - labelStart;
            if (!isValidLabel(text + labelStart, labelLen)){
                return false;
            }
            if (i == len && isAllDigits(text + labelStart, labelLen)){
                return false; /* purely numeric TLD */
            }
            labelStart = i + 1;
        }
    }
    return true;
}
