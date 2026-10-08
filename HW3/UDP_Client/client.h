#ifndef CLIENT_H
#define CLIENT_H

#include <stddef.h>

#define MAX_RESPONSE_LEN 4096

/* Error codes returned by queryServer. */
#define QUERY_TIMEOUT (-1)
#define QUERY_REFUSED (-2)
#define QUERY_ERROR (-3)

/**
 * @function createClientSocket: Create a UDP socket associated with the
 *           server address and set a receive timeout.
 *
 * @param serverIp: Dotted-decimal IPv4 address of the server.
 * @param port: Server port (1-65535).
 *
 * @return: the socket descriptor (>= 0) if success.
 *          -1 if the address is invalid or the socket cannot be created.
 */
int createClientSocket(const char *serverIp, int port);

/**
 * @function queryServer: Send one request and wait for the response.
 *           Replies left over from earlier timed-out requests are discarded
 *           first.
 *
 * @param socketFd: Socket from createClientSocket.
 * @param request: The text to send.
 * @param response: Output buffer; '\0'-terminated on success.
 * @param size: Size of the output buffer in bytes.
 *
 * @return: length of the response (>= 0) if success.
 *          QUERY_TIMEOUT if no response arrived in time.
 *          QUERY_REFUSED if the server is not reachable on that port.
 *          QUERY_ERROR on any other failure.
 */
int queryServer(int socketFd, const char *request, char *response, size_t size);

#endif
