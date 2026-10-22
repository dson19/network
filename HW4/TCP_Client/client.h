#ifndef CLIENT_H
#define CLIENT_H

#include <stddef.h>

#include "stream.h"

#define MAX_RESPONSE_LEN 1024
#define MAX_FILE_SIZE 4294967296ULL /* 2^32 bytes */

/* Outcomes of uploadFile. */
enum {
    UPLOAD_DONE,            /* the server answered: see the response */
    UPLOAD_OPEN_FAILED,     /* local errors: nothing was sent */
    UPLOAD_NOT_REGULAR,
    UPLOAD_TOO_LARGE,
    UPLOAD_INVALID_NAME,
    UPLOAD_READ_FAILED,     /* fatal: the file could not be read while sending */
    UPLOAD_CONNECTION_LOST  /* fatal: the connection to the server is broken */
};

/**
 * @function connectToServer: Create a TCP socket and connect it to the server.
 *
 * @param serverIp: Dotted-decimal IPv4 address of the server.
 * @param port: Server port (1-65535).
 *
 * @return: the connected socket descriptor (>= 0) if success.
 *          -1 if the address is invalid or the connection fails.
 */
int connectToServer(const char *serverIp, int port);

/**
 * @function uploadFile: Upload one file: send "UPLD File_name File_size",
 *           wait for "+OK Please send file", send the content in blocks
 *           while showing the progress, then receive the result.
 *           Only the base name of `path` is sent as File_name.
 *
 * @param conn: The connection to the server.
 * @param path: Path of the local file.
 * @param response: Output buffer for the last server message; valid only
 *        when UPLOAD_DONE is returned.
 * @param size: Size of the output buffer in bytes.
 *
 * @return: UPLOAD_DONE if the server answered (success or rejection).
 *          UPLOAD_OPEN_FAILED, UPLOAD_NOT_REGULAR, UPLOAD_TOO_LARGE or
 *          UPLOAD_INVALID_NAME if the file cannot be sent; nothing was sent.
 *          UPLOAD_READ_FAILED or UPLOAD_CONNECTION_LOST if the transfer
 *          broke; the connection can no longer be used.
 */
int uploadFile(Connection *conn, const char *path, char *response, size_t size);

#endif
