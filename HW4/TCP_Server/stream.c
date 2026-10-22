#include "stream.h"
#include <errno.h>
#include <string.h>
#include <sys/socket.h>

void initConnection(Connection *conn, int fd){
    conn->fd = fd;
    conn->start = 0;
    conn->end = 0;
}

int sendAll(int fd, const void *data, size_t length){
    const char *next = data;
    while (length > 0){
        /* MSG_NOSIGNAL: a closed peer gives EPIPE instead of killing the process. */
        ssize_t sent = send(fd, next, length, MSG_NOSIGNAL);
        if (sent < 0){
            if (errno == EINTR){
                continue;
            }
            return -1;
        }
        next += sent;
        length -= (size_t)sent;
    }
    return 0;
}

int sendMessage(int fd, const char *message){
    char buffer[MAX_MESSAGE_LEN + 2];
    size_t length = strlen(message);
    if (length > MAX_MESSAGE_LEN){
        return -1;
    }

    memcpy(buffer, message, length);
    buffer[length] = '\r';
    buffer[length + 1] = '\n';
    return sendAll(fd, buffer, length + 2);
}

/* recv() once, retrying on signals. Returns the byte count (> 0) or a STREAM_* code. */
static ssize_t receiveSome(int fd, void *data, size_t size){
    for (;;){
        ssize_t received = recv(fd, data, size, 0);
        if (received > 0){
            return received;
        }
        if (received == 0){
            return STREAM_CLOSED;
        }
        if (errno == EINTR){
            continue;
        }
        return STREAM_ERROR;
    }
}

int recvLine(Connection *conn, char *line, size_t size){
    size_t length = 0;
    for (;;){
        if (conn->start == conn->end){
            ssize_t received = receiveSome(conn->fd, conn->buffer, sizeof(conn->buffer));
            if (received < 0){
                line[length] = '\0';
                return (int)received;
            }
            conn->start = 0;
            conn->end = (size_t)received;
        }

        char c = conn->buffer[conn->start++];
        if (c == '\n'){
            break;
        }
        if (length + 1 >= size){
            line[length] = '\0';
            return STREAM_TOO_LONG;
        }
        line[length++] = c;
    }

    if (length > 0 && line[length - 1] == '\r'){
        length--;
    }
    line[length] = '\0';
    return (int)length;
}

ssize_t recvData(Connection *conn, void *data, size_t size){
    size_t buffered = conn->end - conn->start;
    if (buffered == 0){
        return receiveSome(conn->fd, data, size);
    }

    size_t count = (buffered < size) ? buffered : size;
    memcpy(data, conn->buffer + conn->start, count);
    conn->start += count;
    return (ssize_t)count;
}
