#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include "account.h"
#include "server.h"

/* Parse a port number in [1, 65535]. Returns 0 and sets *port if valid, -1 otherwise. */
static int parsePort(const char *text, int *port){
    char *end = NULL;
    long value = strtol(text, &end, 10);
    if (end == text || *end != '\0' || value < 1 || value > 65535){
        return -1;
    }
    *port = (int)value;
    return 0;
}

int main(int argc, char *argv[]){
    if (argc != 2){
        fprintf(stderr, "Usage: %s Port_Number\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port;
    if (parsePort(argv[1], &port) != 0){
        fprintf(stderr, "Error: invalid port number '%s'\n", argv[1]);
        return EXIT_FAILURE;
    }

    if (checkAccountFile(ACCOUNT_FILE) != 0){
        fprintf(stderr, "Error: cannot read account file '%s'\n", ACCOUNT_FILE);
        return EXIT_FAILURE;
    }

    /* A client that disconnects while we write must not kill the server. */
    signal(SIGPIPE, SIG_IGN);
    /* Line buffering: nothing is left in the buffer when fork() copies it. */
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (installChildReaper() != 0){
        return EXIT_FAILURE;
    }

    int listenFd = createServerSocket(port);
    if (listenFd < 0){
        return EXIT_FAILURE;
    }

    printf("Server is listening on TCP port %d, accounts from '%s'\n", port, ACCOUNT_FILE);
    runServerLoop(listenFd, ACCOUNT_FILE);

    return EXIT_SUCCESS;
}
