#include "client.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/* User-facing messages for every response code of the protocol. */
static const struct {
    const char *code;
    const char *message;
} RESPONSE_MESSAGES[] = {
    { "100", "Connected to the message board" },
    { "110", "Logged in successfully" },
    { "120", "Message posted successfully" },
    { "130", "Logged out successfully" },
    { "211", "This account is banned" },
    { "212", "This account does not exist" },
    { "213", "You are already logged in, log out first" },
    { "221", "You are not logged in" },
    { "300", "The server could not identify the request" },
};

int connectToServer(const char *serverIp, int port){
    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons((unsigned short)port);
    if (inet_pton(AF_INET, serverIp, &server.sin_addr) != 1){
        fprintf(stderr, "Error: invalid IP address '%s'\n", serverIp);
        return -1;
    }

    int socketFd = socket(AF_INET, SOCK_STREAM, 0);
    if (socketFd < 0){
        perror("socket");
        return -1;
    }
    if (connect(socketFd, (struct sockaddr *)&server, sizeof(server)) < 0){
        perror("connect");
        close(socketFd);
        return -1;
    }
    return socketFd;
}

int sendRequest(Connection *conn, const char *command, const char *argument,
                char *response, size_t size){
    char request[MAX_MESSAGE_LEN + 1];
    int length = (argument == NULL)
                     ? snprintf(request, sizeof(request), "%s", command)
                     : snprintf(request, sizeof(request), "%s %s", command, argument);
    if (length < 0 || (size_t)length >= sizeof(request)){
        return -1;
    }

    if (sendMessage(conn->fd, request) != 0 || recvLine(conn, response, size) < 0){
        return -1;
    }
    return 0;
}

const char *describeResponse(const char *response){
    size_t count = sizeof(RESPONSE_MESSAGES) / sizeof(RESPONSE_MESSAGES[0]);
    for (size_t i = 0; i < count; i++){
        if (strcmp(response, RESPONSE_MESSAGES[i].code) == 0){
            return RESPONSE_MESSAGES[i].message;
        }
    }
    return "Unknown response from the server";
}
