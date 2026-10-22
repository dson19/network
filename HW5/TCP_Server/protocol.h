#ifndef PROTOCOL_H
#define PROTOCOL_H

#include "stream.h"

#define MAX_LINE_LEN MAX_MESSAGE_LEN

#define LOGIN_COMMAND "USER"
#define POST_COMMAND "POST"
#define LOGOUT_COMMAND "BYE"

/* Response codes sent by the server, one per line. */
#define REPLY_WELCOME "100"
#define REPLY_LOGIN_OK "110"
#define REPLY_POST_OK "120"
#define REPLY_LOGOUT_OK "130"
#define REPLY_ACCOUNT_BANNED "211"
#define REPLY_ACCOUNT_NOT_FOUND "212"
#define REPLY_ALREADY_LOGGED_IN "213"
#define REPLY_NOT_LOGGED_IN "221"
#define REPLY_UNKNOWN_REQUEST "300"

/* Request types returned by parseRequest. */
enum {
    REQUEST_LOGIN,
    REQUEST_POST,
    REQUEST_LOGOUT,
    REQUEST_UNKNOWN
};

/**
 * @function parseRequest: Identify a request line. Keywords are
 *           case-sensitive and separated from the argument by one space:
 *           - "USER username" (an empty username is still a login request,
 *             answered by the account check);
 *           - "POST article" (the article may contain spaces);
 *           - "BYE", with no argument.
 *
 * @param line: The received line, without "\r\n".
 * @param argument: Output pointer into `line` to the argument (username or
 *        article), "" if there is none. Set only for REQUEST_LOGIN and
 *        REQUEST_POST.
 *
 * @return: REQUEST_LOGIN, REQUEST_POST or REQUEST_LOGOUT.
 *          REQUEST_UNKNOWN if the request type cannot be identified.
 */
int parseRequest(const char *line, const char **argument);

#endif
