#ifndef STREAM_H
#define STREAM_H

#include <stdbool.h>
#include <stddef.h>

#define MAX_MESSAGE_LEN 4096
#define MAX_REPLY_LEN 16

/* Results of the receive functions. */
#define STREAM_INCOMPLETE (-1)  /* no complete message yet: try again later */
#define STREAM_CLOSED (-2)
#define STREAM_ERROR (-3)
#define STREAM_TOO_LONG (-4)

/* Bytes received from a non-blocking socket and not used yet. TCP is a byte
 * stream: one recv() may return part of a message or several messages, so
 * messages are cut out of this buffer only once their "\n" has arrived. */
typedef struct {
    char data[MAX_MESSAGE_LEN + 2]; /* the longest message, then "\r\n" */
    size_t length;
    bool overflowed; /* dropping a message that does not fit, up to its "\n" */
} RecvBuffer;

/* Bytes of a reply that the socket has not accepted yet. */
typedef struct {
    char data[MAX_REPLY_LEN + 2];
    size_t start;
    size_t end;
} SendBuffer;

/**
 * @function setNonBlocking: Make recv() and send() on a descriptor return at
 *           once instead of waiting.
 *
 * @param fd: A file descriptor.
 *
 * @return: 0 if success.
 *          -1 if the mode cannot be changed.
 */
int setNonBlocking(int fd);

/**
 * @function hasRecvSpace: Tell whether more bytes can be received.
 *
 * @param in: The receive buffer.
 *
 * @return: true if the buffer is not full.
 */
bool hasRecvSpace(const RecvBuffer *in);

/**
 * @function fillRecvBuffer: Call recv() once and append the bytes to the
 *           buffer. Must be called only if hasRecvSpace is true.
 *
 * @param fd: A connected non-blocking socket.
 * @param in: The receive buffer.
 *
 * @return: 0 if bytes were received, or if none are available right now.
 *          STREAM_CLOSED if the peer closed its side of the connection.
 *          STREAM_ERROR if the connection failed.
 */
int fillRecvBuffer(int fd, RecvBuffer *in);

/**
 * @function hasCompleteLine: Tell whether a whole message is buffered.
 *
 * @param in: The receive buffer.
 *
 * @return: true if takeLine would not return STREAM_INCOMPLETE.
 */
bool hasCompleteLine(const RecvBuffer *in);

/**
 * @function takeLine: Remove one message terminated by "\n" from the buffer
 *           (a "\r" before it is removed too). A message longer than
 *           MAX_MESSAGE_LEN is dropped and reported once its "\n" arrives,
 *           so the next call starts at the following message.
 *
 * @param in: The receive buffer.
 * @param line: Output buffer, '\0'-terminated, of at least
 *        MAX_MESSAGE_LEN + 1 bytes. Set only if the result is >= 0.
 *
 * @return: length of the line (>= 0) if success.
 *          STREAM_INCOMPLETE if no complete message is buffered.
 *          STREAM_TOO_LONG if the message did not fit.
 */
int takeLine(RecvBuffer *in, char *line);

/**
 * @function hasPendingSend: Tell whether a reply is still waiting to be sent.
 *
 * @param out: The send buffer.
 *
 * @return: true if some bytes have not been sent yet.
 */
bool hasPendingSend(const SendBuffer *out);

/**
 * @function queueReply: Put a reply followed by "\r\n" in the send buffer,
 *           which must be empty.
 *
 * @param out: The send buffer.
 * @param reply: The reply text, at most MAX_REPLY_LEN bytes.
 */
void queueReply(SendBuffer *out, const char *reply);

/**
 * @function flushSendBuffer: Send as many pending bytes as the socket
 *           accepts, without waiting. The rest stays in the buffer.
 *
 * @param fd: A connected non-blocking socket.
 * @param out: The send buffer.
 *
 * @return: 0 if the connection is still usable.
 *          STREAM_ERROR if the connection failed.
 */
int flushSendBuffer(int fd, SendBuffer *out);

#endif
