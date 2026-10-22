#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MAX_LINE_LEN 1024
#define MAX_FILE_NAME_LEN 255
#define MAX_FILE_SIZE 4294967296ULL /* 2^32 bytes */

#define UPLOAD_COMMAND "UPLD"

/* Messages sent by the server: '+' marks success, '-' marks an error. */
#define REPLY_WELCOME "+OK Welcome to file server"
#define REPLY_SEND_FILE "+OK Please send file"
#define REPLY_UPLOAD_OK "+OK Successful upload"
#define REPLY_INVALID_COMMAND "-ERR Invalid command"
#define REPLY_COMMAND_TOO_LONG "-ERR Command too long"
#define REPLY_INVALID_NAME "-ERR Invalid file name"
#define REPLY_INVALID_SIZE "-ERR Invalid file size"
#define REPLY_FILE_TOO_LARGE "-ERR File too large (maximum 4294967296 bytes)"
#define REPLY_CANNOT_SAVE "-ERR Cannot save file"

/* Outcomes of parseUploadCommand. */
enum {
    PARSE_OK,
    PARSE_INVALID_COMMAND,
    PARSE_INVALID_NAME,
    PARSE_INVALID_SIZE,
    PARSE_TOO_LARGE
};

/**
 * @function isValidFileName: Check that a client-supplied name is a plain
 *           file name that stays inside the storage directory:
 *           - 1-255 bytes, not "." or "..";
 *           - no '/' and no control characters;
 *           - does not start or end with a space.
 *
 * @param name: The name to check.
 *
 * @return: true if `name` can be used as a file name.
 *          false otherwise (including NULL).
 */
bool isValidFileName(const char *name);

/**
 * @function parseUploadCommand: Parse "UPLD File_name File_size".
 *           The size is the last space-separated token, so the file name
 *           may contain spaces. The size must be decimal digits only and at
 *           most MAX_FILE_SIZE.
 *
 * @param line: The received line, without "\r\n".
 * @param fileName: Output buffer for the file name.
 * @param nameSize: Size of `fileName` in bytes.
 * @param fileSize: Output file size in bytes.
 *
 * @return: PARSE_OK if the command is valid.
 *          PARSE_INVALID_COMMAND if the line is not an UPLD command.
 *          PARSE_INVALID_NAME if the file name is not acceptable.
 *          PARSE_INVALID_SIZE if the size is not a decimal number.
 *          PARSE_TOO_LARGE if the size exceeds MAX_FILE_SIZE.
 */
int parseUploadCommand(const char *line, char *fileName, size_t nameSize, uint64_t *fileSize);

/**
 * @function parseErrorReply: Get the error message for a parse failure.
 *
 * @param status: A PARSE_* code other than PARSE_OK.
 *
 * @return: the matching REPLY_* message.
 */
const char *parseErrorReply(int status);

#endif
