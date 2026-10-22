#include "client.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "util.h"

#define FILE_BLOCK_SIZE (64 * 1024)
#define UPLOAD_COMMAND "UPLD"
#define REPLY_SEND_FILE "+OK Please send file"

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

/* Return 1 if the name can be sent in a command line: no control characters
 * (a '\n' inside the name would split the command in two). */
static int isSendableName(const char *name){
    if (*name == '\0'){
        return 0;
    }
    for (const char *p = name; *p != '\0'; p++){
        unsigned char c = (unsigned char)*p;
        if (c < 0x20 || c == 0x7f){
            return 0;
        }
    }
    return 1;
}

/* Show the transfer progress on one line, redrawn only when the percentage changes. */
static void printProgress(uint64_t sent, uint64_t total, int *lastPercent){
    int percent = (total == 0) ? 100 : (int)(sent * 100 / total);
    if (percent != *lastPercent){
        printf("\rSending... %3d%% (%" PRIu64 "/%" PRIu64 " bytes)", percent, sent, total);
        fflush(stdout);
        *lastPercent = percent;
    }
}

/* Send exactly `fileSize` bytes of the file in blocks.
 * Returns UPLOAD_DONE, UPLOAD_READ_FAILED or UPLOAD_CONNECTION_LOST. */
static int sendFileData(int socketFd, int fileFd, uint64_t fileSize){
    static char block[FILE_BLOCK_SIZE];
    uint64_t sent = 0;
    int lastPercent = -1;
    int status = UPLOAD_DONE;

    printProgress(0, fileSize, &lastPercent);
    while (sent < fileSize){
        uint64_t remaining = fileSize - sent;
        size_t wanted = (remaining < sizeof(block)) ? (size_t)remaining : sizeof(block);

        ssize_t count = read(fileFd, block, wanted);
        if (count < 0 && errno == EINTR){
            continue;
        }
        if (count <= 0){
            status = UPLOAD_READ_FAILED; /* read error, or the file got shorter */
            break;
        }
        if (sendAll(socketFd, block, (size_t)count) != 0){
            status = UPLOAD_CONNECTION_LOST;
            break;
        }
        sent += (uint64_t)count;
        printProgress(sent, fileSize, &lastPercent);
    }
    printf("\n");
    return status;
}

/* Run the upload exchange for an opened file. See uploadFile. */
static int sendOpenedFile(Connection *conn, int fileFd, const char *fileName,
                          char *response, size_t size){
    struct stat info;
    if (fstat(fileFd, &info) != 0 || !S_ISREG(info.st_mode)){
        return UPLOAD_NOT_REGULAR;
    }
    uint64_t fileSize = (uint64_t)info.st_size;
    if (fileSize > MAX_FILE_SIZE){
        return UPLOAD_TOO_LARGE;
    }
    if (!isSendableName(fileName)){
        return UPLOAD_INVALID_NAME;
    }

    char command[MAX_MESSAGE_LEN + 1];
    int length = snprintf(command, sizeof(command), "%s %s %" PRIu64,
                          UPLOAD_COMMAND, fileName, fileSize);
    if (length < 0 || (size_t)length >= sizeof(command)){
        return UPLOAD_INVALID_NAME;
    }

    if (sendMessage(conn->fd, command) != 0 || recvLine(conn, response, size) < 0){
        return UPLOAD_CONNECTION_LOST;
    }
    if (strcmp(response, REPLY_SEND_FILE) != 0){
        return UPLOAD_DONE; /* rejected: the response holds the reason */
    }

    int status = sendFileData(conn->fd, fileFd, fileSize);
    if (status != UPLOAD_DONE){
        return status;
    }
    if (recvLine(conn, response, size) < 0){
        return UPLOAD_CONNECTION_LOST;
    }
    return UPLOAD_DONE;
}

int uploadFile(Connection *conn, const char *path, char *response, size_t size){
    int fileFd = open(path, O_RDONLY);
    if (fileFd < 0){
        return UPLOAD_OPEN_FAILED;
    }

    int status = sendOpenedFile(conn, fileFd, getBaseName(path), response, size);
    close(fileFd);
    return status;
}
