#include "protocol.h"

#include <string.h>

const char *proto_default_msg(int code)
{
    switch (code) {
    case RC_OK:                return "OK";
    case RC_BAD_FORMAT:        return "Bad format";
    case RC_UNKNOWN_CMD:       return "Unknown command";
    case RC_BAD_PARAM:         return "Bad parameter";
    case RC_MSG_TOO_LONG:      return "Message too long";
    case RC_NOT_LOGGED_IN:     return "Not logged in";
    case RC_ALREADY_LOGGED_IN: return "Already logged in";
    case RC_WRONG_CREDENTIALS: return "Wrong username or password";
    case RC_ACCOUNT_LOCKED:    return "Account locked";
    case RC_USERNAME_TAKEN:    return "Username taken";
    case RC_INVALID_USERNAME:  return "Invalid username";
    case RC_INVALID_PASSWORD:  return "Invalid password";
    case RC_INTERNAL_ERROR:    return "Internal error";
    case RC_SERVER_BUSY:       return "Server busy";
    default:                   return "Error";
    }
}

int proto_valid_username(const char *s)
{
    size_t n = strlen(s);
    if (n < USERNAME_MIN || n > USERNAME_MAX)
        return 0;
    for (size_t i = 0; i < n; i++) {
        char c = s[i];
        int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                 (c >= '0' && c <= '9') || c == '_';
        if (!ok)
            return 0;
    }
    return 1;
}

int proto_valid_password(const char *s)
{
    size_t n = strlen(s);
    return n >= PASSWORD_MIN && n <= PASSWORD_MAX;
}
