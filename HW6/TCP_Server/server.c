#include "server.h"
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "session.h"

#define CLIENT_ADDRESS_LEN (INET_ADDRSTRLEN + 8)

/* Stack size of a client thread. A session needs about 16 KiB of stack, so
 * the 8 MiB default would only waste address space with many clients. */
#define CLIENT_THREAD_STACK_SIZE (256 * 1024)

/* Everything a client thread needs. Allocated by the accepting thread and
 * freed by the client thread, so no data is shared between the two. */
typedef struct {
    int connFd;
    char clientAddress[CLIENT_ADDRESS_LEN];
    const char *accountPath;
} ClientTask;

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

/* Return 1 if an accept() or pthread_create() error comes from exhausted resources (worth a pause). */
static int isResourceError(int error){
    return error == EMFILE || error == ENFILE || error == ENOBUFS || error == ENOMEM ||
           error == EAGAIN;
}

/* Thread body: serve one client, then release everything it owns. */
static void *clientThread(void *arg){
    ClientTask *task = arg;

    /* Send each short reply at once instead of letting Nagle's
     * algorithm hold it back to merge it with the next one. */
    int enable = 1;
    setsockopt(task->connFd, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable));

    printf("Client %s connected\n", task->clientAddress);
    handleClient(task->connFd, task->clientAddress, task->accountPath);
    close(task->connFd);
    printf("Client %s disconnected\n", task->clientAddress);

    free(task);
    return NULL;
}

/* Start a detached thread for one accepted connection.
 * Returns 0 if success, or the pthread error code (the caller still owns the task). */
static int startClientThread(ClientTask *task){
    pthread_attr_t attr;
    int error = pthread_attr_init(&attr);
    if (error != 0){
        return error;
    }
    /* Detached: the thread's resources are freed as soon as it returns,
     * nobody has to join it. */
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&attr, CLIENT_THREAD_STACK_SIZE);

    pthread_t thread;
    error = pthread_create(&thread, &attr, clientThread, task);
    pthread_attr_destroy(&attr);
    return error;
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

        ClientTask *task = malloc(sizeof(*task));
        if (task == NULL){
            fprintf(stderr, "Error: out of memory, connection refused\n");
            close(connFd);
            continue;
        }
        task->connFd = connFd;
        task->accountPath = accountPath;
        formatClientAddress(&client, task->clientAddress, sizeof(task->clientAddress));

        int error = startClientThread(task);
        if (error != 0){
            fprintf(stderr, "Error: cannot create a thread for %s: %s\n",
                    task->clientAddress, strerror(error));
            close(connFd); /* the client sees the connection closed */
            free(task);
            if (isResourceError(error)){
                sleep(1);
            }
        }
    }
}
