#include "stream.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

int setNonBlocking(int fd){
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0){
        return -1;
    }
    return 0;
}

bool hasRecvSpace(const RecvBuffer *in){
    return in->length < sizeof(in->data);
}

int fillRecvBuffer(int fd, RecvBuffer *in){
    ssize_t received = recv(fd, in->data + in->length, sizeof(in->data) - in->length, 0);
    if (received > 0){
        in->length += (size_t)received;
        return 0;
    }
    if (received == 0){
        return STREAM_CLOSED;
    }
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR){
        return 0;
    }
    return STREAM_ERROR;
}

bool hasCompleteLine(const RecvBuffer *in){
    return memchr(in->data, '\n', in->length) != NULL;
}

int takeLine(RecvBuffer *in, char *line){
    const char *newline = memchr(in->data, '\n', in->length);
    if (newline == NULL){
        if (!hasRecvSpace(in)){
            in->overflowed = true; /* free the buffer to reach the "\n" */
            in->length = 0;
        }
        return STREAM_INCOMPLETE;
    }

    size_t length = (size_t)(newline - in->data);
    size_t consumed = length + 1;
    if (length > 0 && in->data[length - 1] == '\r'){
        length--;
    }

    bool tooLong = in->overflowed || length > MAX_MESSAGE_LEN;
    if (!tooLong){
        memcpy(line, in->data, length);
        line[length] = '\0';
    }

    in->length -= consumed;
    memmove(in->data, in->data + consumed, in->length);
    in->overflowed = false;
    return tooLong ? STREAM_TOO_LONG : (int)length;
}

bool hasPendingSend(const SendBuffer *out){
    return out->start != out->end;
}

void queueReply(SendBuffer *out, const char *reply){
    size_t length = strlen(reply);
    memcpy(out->data, reply, length);
    out->data[length] = '\r';
    out->data[length + 1] = '\n';
    out->start = 0;
    out->end = length + 2;
}

int flushSendBuffer(int fd, SendBuffer *out){
    while (hasPendingSend(out)){
        /* MSG_NOSIGNAL: a closed peer gives EPIPE instead of killing the process. */
        ssize_t sent = send(fd, out->data + out->start, out->end - out->start, MSG_NOSIGNAL);
        if (sent < 0){
            if (errno == EINTR){
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK){
                return 0; /* the rest is sent when poll() reports POLLOUT */
            }
            return STREAM_ERROR;
        }
        out->start += (size_t)sent;
    }
    return 0;
}
