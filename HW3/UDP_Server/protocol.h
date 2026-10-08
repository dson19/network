#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stddef.h>

#include "util.h"

#define MAX_REQUEST_LEN 1024
#define MAX_RESPONSE_LEN 4096

#define SUCCESS_PREFIX '+'
#define ERROR_PREFIX '-'

/**
 * @function formatSuccessResponse: Build a success response:
 *           '+' followed by the items separated by single spaces.
 *           Items that do not fit in the buffer are dropped whole.
 *
 * @param list: The lookup results (must not be NULL).
 * @param response: Output buffer.
 * @param size: Size of the output buffer in bytes (at least 2).
 *
 * @return: length of the response (without the terminating '\0') if success.
 *          -1 if the arguments are invalid.
 */
int formatSuccessResponse(const StringList *list, char *response, size_t size);

/**
 * @function formatErrorResponse: Build an error response:
 *           '-' followed by the message, cut if it does not fit.
 *
 * @param message: The error text (e.g. "Not found information").
 * @param response: Output buffer.
 * @param size: Size of the output buffer in bytes (at least 2).
 *
 * @return: length of the response (without the terminating '\0') if success.
 *          -1 if the arguments are invalid.
 */
int formatErrorResponse(const char *message, char *response, size_t size);

#endif
