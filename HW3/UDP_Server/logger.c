#include "logger.h"
#include <stdio.h>
#include <time.h>

/* Write `text` to the file, replacing unsafe characters by '?'. */
static void writeSanitized(FILE *file, const char *text){
    for (const char *p = text; *p != '\0'; p++){
        unsigned char c = (unsigned char)*p;
        if (c < 0x20 || c >= 0x7f || c == '$'){
            fputc('?', file);
        } else {
            fputc(c, file);
        }
    }
}

int writeLog(const char *logPath, const char *request, const char *response){
    FILE *file = fopen(logPath, "a");
    if (file == NULL){
        return 1;
    }

    char timestamp[32] = "00/00/0000 00:00:00";
    time_t now = time(NULL);
    struct tm *localTime = localtime(&now);
    if (localTime != NULL){
        strftime(timestamp, sizeof(timestamp), "%d/%m/%Y %H:%M:%S", localTime);
    }

    fprintf(file, "[%s]$", timestamp);
    writeSanitized(file, request);
    fputc('$', file);
    writeSanitized(file, response);
    fputc('\n', file);

    fclose(file);
    return 0;
}
