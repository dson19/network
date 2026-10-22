#include "storage.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define TEMP_FILE_PATTERN ".upload_XXXXXX"
#define DIRECTORY_MODE 0755
#define FILE_MODE 0644

int ensureDirectory(const char *path){
    struct stat info;
    if (stat(path, &info) == 0){
        return S_ISDIR(info.st_mode) ? 0 : -1;
    }
    if (errno != ENOENT){
        return -1;
    }
    return (mkdir(path, DIRECTORY_MODE) == 0) ? 0 : -1;
}

int createTempFile(const char *dir, char *tempPath, size_t size){
    int length = snprintf(tempPath, size, "%s/%s", dir, TEMP_FILE_PATTERN);
    if (length < 0 || (size_t)length >= size){
        return -1;
    }

    int fd = mkstemp(tempPath);
    if (fd < 0){
        return -1;
    }
    /* mkstemp creates the file with mode 0600; make uploads readable like normal files. */
    fchmod(fd, FILE_MODE);
    return fd;
}

int writeAll(int fd, const void *data, size_t length){
    const char *next = data;
    while (length > 0){
        ssize_t written = write(fd, next, length);
        if (written < 0){
            if (errno == EINTR){
                continue;
            }
            return -1;
        }
        next += written;
        length -= (size_t)written;
    }
    return 0;
}

int commitFile(const char *tempPath, const char *dir, const char *fileName){
    char finalPath[MAX_PATH_LEN];
    int length = snprintf(finalPath, sizeof(finalPath), "%s/%s", dir, fileName);
    if (length < 0 || (size_t)length >= sizeof(finalPath)){
        return -1;
    }
    return (rename(tempPath, finalPath) == 0) ? 0 : -1;
}

void discardFile(const char *tempPath){
    unlink(tempPath);
}
