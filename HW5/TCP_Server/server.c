#include "server.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
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

/* SIGCHLD handler: several children may end before it runs (signals are not
 * queued), so collect all finished ones without blocking. */
static void reapChildren(int signo){
    (void)signo;
    int savedErrno = errno;
    while (waitpid(-1, NULL, WNOHANG) > 0){
    }
    errno = savedErrno;
}

int installChildReaper(void){
    struct sigaction action;
    memset(&action, 0, sizeof(action));
    action.sa_handler = reapChildren;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    if (sigaction(SIGCHLD, &action, NULL) != 0){
        perror("sigaction");
        return -1;
    }
    return 0;
}

/* Write a client address as "IPAddress:PortNumber". */
static void formatClientAddress(const struct sockaddr_in *address, char *text, size_t size){
    char ip[INET_ADDRSTRLEN] = "?";
    inet_ntop(AF_INET, &address->sin_addr, ip, sizeof(ip));
    snprintf(text, size, "%s:%u", ip, (unsigned)ntohs(address->sin_port));
}

/* Return 1 if an accept() or fork() error comes from exhausted resources (worth a pause). */
static int isResourceError(int error){
    return error == EMFILE || error == ENFILE || error == ENOBUFS || error == ENOMEM ||
           error == EAGAIN;
}

void runServerLoop(int listenFd, const char *accountPath){
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

        pid_t pid = fork();
        if (pid < 0){
            int error = errno;
            perror("fork");
            close(connFd); /* the client sees the connection closed */
            if (isResourceError(error)){
                sleep(1);
            }
            continue;
        }

        if (pid == 0){
            /* Child: serve this client only, then exit. */
            close(listenFd);
            /* Send each short reply at once instead of letting Nagle's
             * algorithm hold it back to merge it with the next one. */
            int enable = 1;
            setsockopt(connFd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));
            printf("Client %s connected (process %ld)\n", clientAddress, (long)getpid());
            handleClient(connFd, clientAddress, accountPath);
            close(connFd);
            printf("Client %s disconnected (process %ld)\n", clientAddress, (long)getpid());
            exit(EXIT_SUCCESS);
        }

        /* Parent: the child owns the connection now. */
        close(connFd);
    }
}
