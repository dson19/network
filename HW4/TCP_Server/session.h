#ifndef SESSION_H
#define SESSION_H

/**
 * @function handleClient: Serve one connected client until it disconnects:
 *           send the welcome message, then for every "UPLD File_name
 *           File_size" command reply "+OK Please send file", receive the
 *           file into the storage directory and reply with the result.
 *           Every request is logged. Invalid commands get an "-ERR" reply
 *           and the session goes on; the session ends when the client
 *           disconnects, even in the middle of a transfer.
 *
 * @param connFd: The connected client socket (closed by the caller).
 * @param clientAddress: Client address as "IPAddress:PortNumber".
 * @param storageDir: Directory where uploaded files are stored.
 * @param logPath: Path of the log file.
 */
void handleClient(int connFd, const char *clientAddress, const char *storageDir,
                  const char *logPath);

#endif
