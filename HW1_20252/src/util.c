/* Needed for getline(), which is a POSIX extension, not plain C11. */
#define _POSIX_C_SOURCE 200809L

#include "util.h"

#include <stdio.h>
#include <stdlib.h>

char *read_line(void) {
    char *line = NULL;
    size_t buf_size = 0;

    /* getline() allocates/grows `line` as needed, so there is no fixed
     * length limit on what the user can type. */
    ssize_t len = getline(&line, &buf_size, stdin);
    if (len == -1) {
        free(line);
        return NULL;
    }

    /* Strip a trailing "\n" or "\r\n" so callers never see line endings. */
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }

    return line;
}
