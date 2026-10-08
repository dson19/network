#include "server.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "handler.h"
#include "logger.h"
#include "protocol.h"

#define RECEIVE_BUFFER_BYTES (1 << 20)

int createServerSocket(int port){
    int socketFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketFd < 0){
        perror("socket");
        return -1;
    }

    int enable = 1;
    setsockopt(socketFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    /* A larger queue keeps bursts of requests from being dropped. */
    int bufferBytes = RECEIVE_BUFFER_BYTES;
    setsockopt(socketFd, SOL_SOCKET, SO_RCVBUF, &bufferBytes, sizeof(bufferBytes));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((unsigned short)port);

    if (bind(socketFd, (struct sockaddr *)&address, sizeof(address)) < 0){
        perror("bind");
        close(socketFd);
        return -1;
    }
    return socketFd;
}

/* Check and clean up a received datagram in place: reject oversized requests
 * and embedded '\0', terminate the text and strip trailing CR/LF.
 * Returns NULL if the request is acceptable, otherwise the error message. */
static const char *prepareRequest(char *buffer, size_t length){
    if (length > MAX_REQUEST_LEN){
        buffer[MAX_REQUEST_LEN] = '\0';
        return "Request too long";
    }
    if (memchr(buffer, '\0', length) != NULL){
        return "Invalid request";
    }

    buffer[length] = '\0';
    while (length > 0 && (buffer[length - 1] == '\n' || buffer[length - 1] == '\r')){
        buffer[--length] = '\0';
    }
    if (length == 0){
        return "Invalid request";
    }
    return NULL;
}

void runServerLoop(int socketFd, const char *logPath){
    char request[MAX_REQUEST_LEN + 1];
    char response[MAX_RESPONSE_LEN];

    for (;;){
        struct sockaddr_in client;
        socklen_t clientLen = sizeof(client);

        ssize_t received = recvfrom(socketFd, request, sizeof(request), 0,
                                    (struct sockaddr *)&client, &clientLen);
        if (received < 0){
            if (errno != EINTR){
                perror("recvfrom");
                sleep(1); /* avoid spinning on a persistent error */
            }
            continue;
        }

        const char *error = prepareRequest(request, (size_t)received);
        if (error != NULL){
            formatErrorResponse(error, response, sizeof(response));
        } else {
            handleQuery(request, response, sizeof(response));
        }

        if (sendto(socketFd, response, strlen(response), 0,
                   (struct sockaddr *)&client, clientLen) < 0){
            perror("sendto");
        }
        if (writeLog(logPath, request, response) != 0){
            fprintf(stderr, "Warning: could not write to log file '%s'\n", logPath);
        }
    }
}
