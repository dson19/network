/* Needed for localtime_r(), which is a POSIX extension, not plain C11. */
#define _POSIX_C_SOURCE 200809L

#include "logger.h"

#include <stdio.h>
#include <time.h>

int logger_write(const char *log_path, int function, const char *value, LogStatus status) {
    /* Opened and closed on every call (instead of kept open for the whole
     * program) so a partially written entry is never left behind if the
     * program exits unexpectedly, and so the write failure is always
     * detected right when it happens. */
    FILE *fp = fopen(log_path, "a");
    if (!fp) {
        return -1;
    }

    time_t now = time(NULL);
    struct tm local_time;
    localtime_r(&now, &local_time); /* thread-safe alternative to localtime() */

    char timestamp[20]; /* "dd/mm/yyyy hh:mm:ss" + '\0' = 20 bytes */
    strftime(timestamp, sizeof(timestamp), "%d/%m/%Y %H:%M:%S", &local_time);

    const char *code = (status == LOG_OK) ? "+OK" : "-ERR";

    fprintf(fp, "[%s] $ %d $ %s $ %s\n", timestamp, function, value, code);

    fclose(fp);
    return 0;
}
