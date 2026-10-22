#include "logger.h"
#include <stdio.h>

/* Write `text` to the file, escaping CR/LF and replacing unsafe characters by '?'. */
static void writeEscaped(FILE *file, const char *text){
    for (const char *p = text; *p != '\0'; p++){
        unsigned char c = (unsigned char)*p;
        if (c == '\r'){
            fputs("\\r", file);
        } else if (c == '\n'){
            fputs("\\n", file);
        } else if (c < 0x20 || c == 0x7f || c == '$'){
            fputc('?', file);
        } else {
            fputc(c, file);
        }
    }
}

int writeLog(const char *logPath, time_t when, const char *clientAddress,
             const char *request, const char *result){
    FILE *file = fopen(logPath, "a");
    if (file == NULL){
        return 1;
    }

    char timestamp[32] = "00/00/0000 00:00:00";
    struct tm *localTime = localtime(&when);
    if (localTime != NULL){
        strftime(timestamp, sizeof(timestamp), "%d/%m/%Y %H:%M:%S", localTime);
    }

    fprintf(file, "[%s]$", timestamp);
    writeEscaped(file, clientAddress);
    fputc('$', file);
    if (request != NULL){
        writeEscaped(file, request);
        fputc('$', file);
    }
    writeEscaped(file, result);
    fputc('\n', file);

    fclose(file);
    return 0;
}
