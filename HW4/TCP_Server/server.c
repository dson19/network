#include "server.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "session.h"

#define CLIENT_ADDRESS_LEN (INET_ADDRSTRLEN + 8)

int createServerSocket(int port){
    int listenFd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenFd < 0){
        perror("socket");
        return -1;
    }

    int enable = 1;
    setsockopt(listenFd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));

    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons((unsigned short)port);

    if (bind(listenFd, (struct sockaddr *)&address, sizeof(address)) < 0){
        perror("bind");
        close(listenFd);
        return -1;
    }
    if (listen(listenFd, SOMAXCONN) < 0){
        perror("listen");
        close(listenFd);
        return -1;
    }
    return listenFd;
}

/* Write a client address as "IPAddress:PortNumber". */
static void formatClientAddress(const struct sockaddr_in *address, char *text, size_t size){
    char ip[INET_ADDRSTRLEN] = "?";
    inet_ntop(AF_INET, &address->sin_addr, ip, sizeof(ip));
    snprintf(text, size, "%s:%u", ip, (unsigned)ntohs(address->sin_port));
}

/* Return 1 if an accept() error comes from exhausted resources (worth a pause). */
static int isResourceError(int error){
    return error == EMFILE || error == ENFILE || error == ENOBUFS || error == ENOMEM;
}

void runServerLoop(int listenFd, const char *storageDir, const char *logPath){
    for (;;){
        struct sockaddr_in client;
        socklen_t clientLen = sizeof(client);

        int connFd = accept(listenFd, (struct sockaddr *)&client, &clientLen);
        if (connFd < 0){
            if (errno != EINTR && errno != ECONNABORTED){
                perror("accept");
                if (isResourceError(errno)){
                    sleep(1); /* avoid spinning until resources are released */
                }
            }
            continue;
        }

        char clientAddress[CLIENT_ADDRESS_LEN];
        formatClientAddress(&client, clientAddress, sizeof(clientAddress));
        printf("Client %s connected\n", clientAddress);

        handleClient(connFd, clientAddress, storageDir, logPath);

        close(connFd);
        printf("Client %s disconnected\n", clientAddress);
    }
}
