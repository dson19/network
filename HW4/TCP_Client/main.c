#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "client.h"
#include "stream.h"
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

/* Upload one file and show the outcome to the user.
 * Returns 1 if the connection can still be used, 0 otherwise. */
static int handleUpload(Connection *conn, const char *path){
    char response[MAX_RESPONSE_LEN];
    int status = uploadFile(conn, path, response, sizeof(response));

    switch (status){
        case UPLOAD_DONE:
            printResponse(response);
            return 1;
        case UPLOAD_OPEN_FAILED:
            printf("Cannot open '%s': %s\n", path, strerror(errno));
            return 1;
        case UPLOAD_NOT_REGULAR:
            printf("'%s' is not a regular file\n", path);
            return 1;
        case UPLOAD_TOO_LARGE:
            printf("File is too large (maximum %llu bytes)\n", MAX_FILE_SIZE);
            return 1;
        case UPLOAD_INVALID_NAME:
            printf("File name is not supported\n");
            return 1;
        case UPLOAD_READ_FAILED:
            printf("Error while reading '%s', the transfer is aborted\n", path);
            return 0;
        default:
            printf("Lost connection to the server\n");
            return 0;
    }
}

int main(int argc, char *argv[]){
    if (argc != 3){
        fprintf(stderr, "Usage: %s IP_Addr Port_Number\n", argv[0]);
        return EXIT_FAILURE;
    }

    int port;
    if (parsePort(argv[2], &port) != 0){
        fprintf(stderr, "Error: invalid port number '%s'\n", argv[2]);
        return EXIT_FAILURE;
    }

    /* A server that disconnects while we write must not kill the client. */
    signal(SIGPIPE, SIG_IGN);

    int socketFd = connectToServer(argv[1], port);
    if (socketFd < 0){
        return EXIT_FAILURE;
    }
    printf("Connected to %s:%d, waiting for the server...\n", argv[1], port);

    Connection conn;
    initConnection(&conn, socketFd);

    char welcome[MAX_RESPONSE_LEN];
    if (recvLine(&conn, welcome, sizeof(welcome)) < 0){
        printf("The server closed the connection\n");
        close(socketFd);
        return EXIT_FAILURE;
    }
    printResponse(welcome);

    char path[MAX_PATH_LEN + 2];
    for (;;){
        printf("Enter file path (empty to quit): ");
        fflush(stdout);

        int status = readLine(path, sizeof(path));
        if (status == READ_EOF){
            printf("\n");
            break;
        }
        if (status == READ_TOO_LONG){
            printf("Path is too long (maximum %d characters)\n", MAX_PATH_LEN);
            continue;
        }
        if (path[0] == '\0'){
            break;
        }
        if (!handleUpload(&conn, path)){
            break;
        }
    }

    close(socketFd);
    return EXIT_SUCCESS;
}
