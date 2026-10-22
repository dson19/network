#ifndef SERVER_H
#define SERVER_H

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
 * @function installChildReaper: Install a SIGCHLD handler that collects
 *           every finished child process, so that no zombie is left behind.
 *
 * @return: 0 if success.
 *          -1 if the handler cannot be installed.
 */
int installChildReaper(void);

/**
 * @function runServerLoop: Accept clients forever and serve each one in its
 *           own child process (fork), so that many clients are served at
 *           the same time. The parent only accepts; errors on a single
 *           connection, including a failed fork, never stop the loop.
 *
 * @param listenFd: A listening socket (see createServerSocket).
 * @param accountPath: Path of the account file.
 */
void runServerLoop(int listenFd, const char *accountPath);

#endif
