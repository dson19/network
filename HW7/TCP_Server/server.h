#ifndef SERVER_H
#define SERVER_H

/**
 * @function raiseFileLimit: Raise this process's limit of open descriptors
 *           to the highest value allowed, since each client uses one.
 *
 * @return: the limit now in force, or -1 if it cannot be read.
 */
long raiseFileLimit(void);

/**
 * @function createServerSocket: Create a TCP socket, bind it to the given
 *           port on all local interfaces and start listening.
 *
 * @param port: Port number (1-65535).
 *
 * @return: the listening socket descriptor (>= 0) if success.
 *          -1 if the socket cannot be created, bound or put in listen mode.
 */
int createServerSocket(int port);

/**
 * @function runServerLoop: Accept clients forever and hand each one to a
 *           worker thread, which serves it with poll(). Errors on a single
 *           connection never stop the loop.
 *
 * @param listenFd: A listening socket (see createServerSocket).
 */
void runServerLoop(int listenFd);

#endif
