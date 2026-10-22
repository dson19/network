#ifndef STREAM_H
#define STREAM_H

#include <stddef.h>
#include <sys/types.h>

#define STREAM_BUFFER_SIZE 4096
#define MAX_MESSAGE_LEN 1024

/* Error codes returned by the receive functions. */
#define STREAM_CLOSED (-1)
#define STREAM_ERROR (-2)
#define STREAM_TOO_LONG (-3)

/* A TCP connection with its own receive buffer. TCP is a byte stream: one
 * recv() may return part of a message or several messages at once, so the
 * bytes read past the end of a line are kept here for the next call. */
typedef struct {
    int fd;
    char buffer[STREAM_BUFFER_SIZE];
    size_t start;
    size_t end;
} Connection;

/**
 * @function initConnection: Attach a connected socket to an empty buffer.
 *
 * @param conn: The connection to initialize (must not be NULL).
 * @param fd: A connected TCP socket.
 */
void initConnection(Connection *conn, int fd);

/**
 * @function sendAll: Send exactly `length` bytes, retrying partial sends.
 *           A peer that has closed the connection gives an error instead of
 *           raising SIGPIPE.
 *
 * @param fd: A connected TCP socket.
 * @param data: The bytes to send.
 * @param length: Number of bytes to send.
 *
 * @return: 0 if every byte was sent.
 *          -1 if the connection failed.
 */
int sendAll(int fd, const void *data, size_t length);

/**
 * @function sendMessage: Send a protocol message followed by "\r\n".
 *
 * @param fd: A connected TCP socket.
 * @param message: The message text, without line break.
 *
 * @return: 0 if the message was sent.
 *          -1 if the message is longer than MAX_MESSAGE_LEN or the
 *          connection failed.
 */
int sendMessage(int fd, const char *message);

/**
 * @function recvLine: Receive one message terminated by "\n" (a "\r" before
 *           it is removed too). Bytes after the line stay in the buffer.
 *
 * @param conn: The connection to read from.
 * @param line: Output buffer, always '\0'-terminated. On STREAM_TOO_LONG it
 *        holds the first `size` - 1 bytes of the line.
 * @param size: Size of the output buffer in bytes (at least 1).
 *
 * @return: length of the line (>= 0) if success.
 *          STREAM_CLOSED if the peer closed the connection first.
 *          STREAM_TOO_LONG if the line does not fit in the buffer.
 *          STREAM_ERROR on any other failure.
 */
int recvLine(Connection *conn, char *line, size_t size);

/**
 * @function recvData: Receive up to `size` raw bytes: bytes already buffered
 *           by recvLine first, then directly from the socket.
 *
 * @param conn: The connection to read from.
 * @param data: Output buffer.
 * @param size: Maximum number of bytes to receive (> 0).
 *
 * @return: number of bytes received (> 0) if success.
 *          STREAM_CLOSED or STREAM_ERROR otherwise.
 */
ssize_t recvData(Connection *conn, void *data, size_t size);

#endif
