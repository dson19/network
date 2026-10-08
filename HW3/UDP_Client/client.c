#include "client.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define RECEIVE_TIMEOUT_SECONDS 5

int createClientSocket(const char *serverIp, int port){
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, serverIp, &server.sin_addr) != 1){
        fprintf(stderr, "Error: invalid IP address '%s'\n", serverIp);
        return -1;
    }

    int socketFd = socket(AF_INET, SOCK_DGRAM, 0);
    if (socketFd < 0){
        perror("socket");
        return -1;
    }

    struct timeval timeout = { RECEIVE_TIMEOUT_SECONDS, 0 };
    setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    /* connect() on UDP only fixes the peer: datagrams from other senders are
     * dropped by the kernel and "port unreachable" errors are reported. */
    if (connect(socketFd, (struct sockaddr *)&server, sizeof(server)) < 0){
        perror("connect");
        close(socketFd);
        return -1;
    }
    return socketFd;
}

/* Discard every datagram (or pending socket error) already waiting on the socket. */
static void drainPendingResponses(int socketFd){
    char discard[MAX_RESPONSE_LEN];
    while (recv(socketFd, discard, sizeof(discard), MSG_DONTWAIT) >= 0){
    }
}

int queryServer(int socketFd, const char *request, char *response, size_t size){
    drainPendingResponses(socketFd);

    if (send(socketFd, request, strlen(request), 0) < 0){
        return (errno == ECONNREFUSED) ? QUERY_REFUSED : QUERY_ERROR;
    }

    ssize_t length = recv(socketFd, response, size - 1, 0);
    if (length < 0){
        if (errno == EAGAIN || errno == EWOULDBLOCK){
            return QUERY_TIMEOUT;
        }
        return (errno == ECONNREFUSED) ? QUERY_REFUSED : QUERY_ERROR;
    }

    response[length] = '\0';
    return (int)length;
}
