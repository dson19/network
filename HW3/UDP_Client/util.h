#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

#define MAX_REQUEST_LEN 1024

/* Outcomes of readLine. */
enum {
    READ_OK,
    READ_EOF,
    READ_TOO_LONG
};

/**
 * @function readLine: Read one line from stdin into `buffer`, removing the
 *           trailing line break. If the line is longer than the buffer, the
 *           rest of the line is discarded.
 *
 * @param buffer: Output buffer (must hold MAX_REQUEST_LEN + 2 bytes).
 * @param size: Size of the output buffer in bytes.
 *
 * @return: READ_OK if a line was read.
 *          READ_EOF on end of input or read error (e.g. Ctrl+D).
 *          READ_TOO_LONG if the line did not fit.
 */
int readLine(char *buffer, size_t size);

/**
 * @function printResponse: Display a server response according to its prefix.
 *           "+a b c" prints "Result:" and one item per line;
 *           "-message" prints the message; any other text is reported as
 *           an invalid response.
 *
 * @param response: The response text, '\0'-terminated.
 */
void printResponse(const char *response);

#endif
