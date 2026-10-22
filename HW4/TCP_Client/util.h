#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

#define MAX_PATH_LEN 4096

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
 * @param buffer: Output buffer (must hold MAX_PATH_LEN + 2 bytes).
 * @param size: Size of the output buffer in bytes.
 *
 * @return: READ_OK if a line was read.
 *          READ_EOF on end of input or read error (e.g. Ctrl+D).
 *          READ_TOO_LONG if the line did not fit.
 */
int readLine(char *buffer, size_t size);

/**
 * @function getBaseName: Get the file name part of a path,
 *           e.g. "dir/sub/test.txt" -> "test.txt".
 *
 * @param path: The path.
 *
 * @return: pointer into `path` just after the last '/', or `path` itself
 *          if it has no '/'.
 */
const char *getBaseName(const char *path);

/**
 * @function printResponse: Display a server message according to its prefix:
 *           "+..." and "-..." are shown as they are; any other text is
 *           reported as an invalid response.
 *
 * @param response: The message text, '\0'-terminated.
 */
void printResponse(const char *response);

#endif
