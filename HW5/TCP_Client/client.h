#ifndef CLIENT_H
#define CLIENT_H

#include <stddef.h>

#include "stream.h"

#define MAX_RESPONSE_LEN 1024

#define LOGIN_COMMAND "USER"
#define POST_COMMAND "POST"
#define LOGOUT_COMMAND "BYE"

/* Response codes defined by the protocol. */
#define REPLY_WELCOME "100"
#define REPLY_LOGIN_OK "110"
#define REPLY_POST_OK "120"
#define REPLY_LOGOUT_OK "130"

/* Longest username / article that fits in one request line. */
#define MAX_USERNAME_LEN (MAX_MESSAGE_LEN - (int)sizeof(LOGIN_COMMAND))
#define MAX_ARTICLE_LEN (MAX_MESSAGE_LEN - (int)sizeof(POST_COMMAND))

/**
 * @function connectToServer: Create a TCP socket and connect it to the server.
 *
 * @param serverIp: Dotted-decimal IPv4 address of the server.
 * @param port: Server port (1-65535).
 *
 * @return: the connected socket descriptor (>= 0) if success.
 *          -1 if the address is invalid or the connection fails.
 */
int connectToServer(const char *serverIp, int port);

/**
 * @function sendRequest: Send one request line built from a command and an
 *           optional argument ("COMMAND" or "COMMAND argument"), then wait
 *           for the server's response line.
 *
 * @param conn: The connection to the server.
 * @param command: The request keyword (e.g. "USER").
 * @param argument: The argument, or NULL for a request without one.
 * @param response: Output buffer for the response code.
 * @param size: Size of the output buffer in bytes.
 *
 * @return: 0 if a response was received.
 *          -1 if the request is too long or the connection is lost.
 */
int sendRequest(Connection *conn, const char *command, const char *argument,
                char *response, size_t size);

/**
 * @function describeResponse: Get a user-facing explanation of a response code.
 *
 * @param response: The response line received from the server.
 *
 * @return: a constant message (never NULL).
 */
const char *describeResponse(const char *response);

#endif
