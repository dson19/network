#include <stdio.h>
#include <stdlib.h>

#include "server.h"

#define STUDENT_ID "20235994"

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
        fprintf(stderr, "Usage: %s PortNumber\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port;
    if (parsePort(argv[1], &port) != 0){
        fprintf(stderr, "Error: invalid port number '%s'\n", argv[1]);
        return EXIT_FAILURE;
    }

    int socketFd = createServerSocket(port);
    if (socketFd < 0){
        return EXIT_FAILURE;
    }

    char logPath[64];
    snprintf(logPath, sizeof(logPath), "log_%s.txt", STUDENT_ID);

    printf("Server is listening on UDP port %d\n", port);
    runServerLoop(socketFd, logPath);

    return EXIT_SUCCESS;
}
