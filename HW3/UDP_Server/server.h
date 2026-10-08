#ifndef SERVER_H
#define SERVER_H

/**
 * @function createServerSocket: Create a UDP socket and bind it to the
 *           given port on all local interfaces.
 *
 * @param port: Port number (1-65535).
 *
 * @return: the socket descriptor (>= 0) if success.
 *          -1 if the socket cannot be created or bound.
 */
int createServerSocket(int port);

/**
 * @function runServerLoop: Serve requests forever. For every datagram it
 *           validates the request, resolves it, replies to the sender and
 *           appends a line to the log file. Errors on a single request are
 *           reported on stderr and never stop the loop.
 *
 * @param socketFd: A bound UDP socket (see createServerSocket).
 * @param logPath: Path of the log file.
 */
void runServerLoop(int socketFd, const char *logPath);

#endif
