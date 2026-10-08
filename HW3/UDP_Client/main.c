#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "client.h"
#include "util.h"

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

/* Send one request and show the outcome to the user. */
static void handleRequest(int socketFd, const char *request){
    char response[MAX_RESPONSE_LEN];
    int length = queryServer(socketFd, request, response, sizeof(response));

    switch (length){
        case QUERY_TIMEOUT:
            printf("No response from server (timeout)\n");
            break;
        case QUERY_REFUSED:
            printf("Cannot reach the server (connection refused)\n");
            break;
        case QUERY_ERROR:
            printf("Communication error\n");
            break;
        default:
            printResponse(response);
            break;
    }
}

int main(int argc, char *argv[]){
    if (argc != 3){
        fprintf(stderr, "Usage: %s IPAddress PortNumber\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port;
    if (parsePort(argv[2], &port) != 0){
        fprintf(stderr, "Error: invalid port number '%s'\n", argv[2]);
        return EXIT_FAILURE;
    }

    int socketFd = createClientSocket(argv[1], port);
    if (socketFd < 0){
        return EXIT_FAILURE;
    }

    char request[MAX_REQUEST_LEN + 2];
    for (;;){
        printf("Enter domain name or IP address: ");
        fflush(stdout);

        int status = readLine(request, sizeof(request));
        if (status == READ_EOF){
            printf("\n");
            break;
        }
        if (status == READ_TOO_LONG){
            printf("Input is too long (maximum %d characters)\n", MAX_REQUEST_LEN);
            continue;
        }
        if (request[0] == '\0'){
            break;
        }
        handleRequest(socketFd, request);
    }

    close(socketFd);
    return EXIT_SUCCESS;
}
