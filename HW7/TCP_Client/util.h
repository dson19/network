#ifndef UTIL_H
#define UTIL_H

#include <stddef.h>

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
 * @param buffer: Output buffer.
 * @param size: Size of the output buffer in bytes (at least 2).
 *
 * @return: READ_OK if a line was read.
 *          READ_EOF on end of input or read error (e.g. Ctrl+D).
 *          READ_TOO_LONG if the line did not fit.
 */
int readLine(char *buffer, size_t size);

/**
 * @function trimSpaces: Remove leading and trailing spaces and tabs in place.
 *
 * @param text: The text to trim.
 *
 * @return: pointer to the first non-blank character of `text`.
 */
char *trimSpaces(char *text);

#endif
