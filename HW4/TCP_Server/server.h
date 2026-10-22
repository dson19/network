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
 * @function runServerLoop: Accept and serve clients forever, one after the
 *           other. Clients that connect while another one is being served
 *           wait in the listen queue. Errors on a single connection are
 *           reported on stderr and never stop the loop.
 *
 * @param listenFd: A listening socket (see createServerSocket).
 * @param storageDir: Directory where uploaded files are stored.
 * @param logPath: Path of the log file.
 */
void runServerLoop(int listenFd, const char *storageDir, const char *logPath);

#endif
