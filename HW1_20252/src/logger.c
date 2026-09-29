/* Cần cho setenv()/tzset() (hàm POSIX), không có sẵn dưới -std=c11 nếu
 * thiếu macro này. Phải đặt trước MỌI #include. */
#define _POSIX_C_SOURCE 200809L

#include "logger.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

int writeLog(const char *logPath, int function, const char *value, bool success){
    FILE *fptr = fopen(logPath, "a");
    if (fptr == NULL){
        return 1;
    }

    /* Ép cứng múi giờ Việt Nam, không phụ thuộc múi giờ cấu hình của máy
     * đang chạy chương trình (VD máy chấm bài có thể đặt UTC). */
    setenv("TZ", "Asia/Ho_Chi_Minh", 1);
    tzset();

    time_t now = time(NULL);
    struct tm *localTime = localtime(&now);

    char timestamp[20]; 
    strftime(timestamp, sizeof(timestamp), "%d/%m/%Y %H:%M:%S", localTime);

    fprintf(fptr, "[%s] $ %d $ %s $ %s\n", timestamp, function, value, success ? "+OK" : "-ERR");

    fclose(fptr);
    return 0;
}
