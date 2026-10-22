#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>

#define MAX_PATH_LEN 4096

/**
 * @function ensureDirectory: Make sure `path` is a directory, creating it
 *           if it does not exist (its parent must exist).
 *
 * @param path: The directory path.
 *
 * @return: 0 if the directory exists or was created.
 *          -1 if it cannot be created or `path` is not a directory.
 */
int ensureDirectory(const char *path);

/**
 * @function createTempFile: Create a new, empty, uniquely named file inside
 *           `dir`. Uploads are written there first and renamed when
 *           complete, so a failed upload never damages an existing file.
 *
 * @param dir: The storage directory.
 * @param tempPath: Output buffer for the path of the created file.
 * @param size: Size of `tempPath` in bytes.
 *
 * @return: the file descriptor (>= 0), open for writing, if success.
 *          -1 if the file cannot be created.
 */
int createTempFile(const char *dir, char *tempPath, size_t size);

/**
 * @function writeAll: Write exactly `length` bytes to a file, retrying
 *           partial writes.
 *
 * @param fd: A file descriptor open for writing.
 * @param data: The bytes to write.
 * @param length: Number of bytes to write.
 *
 * @return: 0 if every byte was written.
 *          -1 on a write error (e.g. disk full).
 */
int writeAll(int fd, const void *data, size_t length);

/**
 * @function commitFile: Move a completed temporary file to dir/fileName,
 *           replacing any existing file of that name (overwrite mode).
 *
 * @param tempPath: Path from createTempFile.
 * @param dir: The storage directory.
 * @param fileName: The final file name (see isValidFileName).
 *
 * @return: 0 if the file was saved.
 *          -1 otherwise (the temporary file is left in place).
 */
int commitFile(const char *tempPath, const char *dir, const char *fileName);

/**
 * @function discardFile: Delete a temporary file of an upload that failed.
 *
 * @param tempPath: Path from createTempFile.
 */
void discardFile(const char *tempPath);

#endif
