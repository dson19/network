#include "session.h"
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "logger.h"
#include "protocol.h"
#include "storage.h"
#include "stream.h"

#define FILE_BLOCK_SIZE (64 * 1024)
#define MAX_RESULT_LEN 128

/* Outcomes of receiveFileData, besides the STREAM_* error codes. */
#define TRANSFER_OK 0
#define TRANSFER_WRITE_FAILED 1

/* State of one client session, shared by the helpers below. */
typedef struct {
    Connection conn;
    const char *clientAddress;
    const char *storageDir;
    const char *logPath;
} Session;

/* Append one entry to the log file, warning on stderr if it fails. */
static void logRequest(const Session *session, time_t when, const char *request, const char *result){
    if (writeLog(session->logPath, when, session->clientAddress, request, result) != 0){
        fprintf(stderr, "Warning: could not write to log file '%s'\n", session->logPath);
    }
}

/* Send a reply and log it. Returns 0 if the reply was sent, -1 otherwise. */
static int replyAndLog(Session *session, time_t when, const char *request, const char *reply){
    int status = sendMessage(session->conn.fd, reply);
    logRequest(session, when, request, reply);
    return status;
}

/* Receive exactly `fileSize` bytes and write them to `fileFd`. After a write
 * error the remaining bytes are still received (and dropped), so the next
 * command is read from the right place in the stream.
 * Sets *received to the number of bytes received.
 * Returns TRANSFER_OK, TRANSFER_WRITE_FAILED or a STREAM_* error code. */
static int receiveFileData(Connection *conn, int fileFd, uint64_t fileSize, uint64_t *received){
    static char block[FILE_BLOCK_SIZE];
    int writeFailed = 0;

    *received = 0;
    while (*received < fileSize){
        uint64_t remaining = fileSize - *received;
        size_t wanted = (remaining < sizeof(block)) ? (size_t)remaining : sizeof(block);

        ssize_t count = recvData(conn, block, wanted);
        if (count < 0){
            return (int)count;
        }
        if (!writeFailed && writeAll(fileFd, block, (size_t)count) != 0){
            writeFailed = 1;
        }
        *received += (uint64_t)count;
    }
    return writeFailed ? TRANSFER_WRITE_FAILED : TRANSFER_OK;
}

/* Handle one "UPLD File_name File_size" request.
 * Returns 0 to keep serving the client, -1 to close the connection. */
static int handleUpload(Session *session, const char *line, const char *request, time_t when){
    char fileName[MAX_FILE_NAME_LEN + 1];
    uint64_t fileSize = 0;
    int parseStatus = parseUploadCommand(line, fileName, sizeof(fileName), &fileSize);
    if (parseStatus != PARSE_OK){
        return replyAndLog(session, when, request, parseErrorReply(parseStatus));
    }

    char tempPath[MAX_PATH_LEN];
    int fileFd = -1;
    if (ensureDirectory(session->storageDir) == 0){
        fileFd = createTempFile(session->storageDir, tempPath, sizeof(tempPath));
    }
    if (fileFd < 0){
        fprintf(stderr, "Error: cannot create a file in '%s'\n", session->storageDir);
        return replyAndLog(session, when, request, REPLY_CANNOT_SAVE);
    }

    if (sendMessage(session->conn.fd, REPLY_SEND_FILE) != 0){
        close(fileFd);
        discardFile(tempPath);
        logRequest(session, when, request, "-ERR Connection closed");
        return -1;
    }

    uint64_t received = 0;
    int status = receiveFileData(&session->conn, fileFd, fileSize, &received);

    if (close(fileFd) != 0 && status == TRANSFER_OK){
        status = TRANSFER_WRITE_FAILED;
    }
    if (status == TRANSFER_OK && commitFile(tempPath, session->storageDir, fileName) == 0){
        return replyAndLog(session, when, request, REPLY_UPLOAD_OK);
    }
    discardFile(tempPath);

    if (status == TRANSFER_OK || status == TRANSFER_WRITE_FAILED){
        fprintf(stderr, "Error: cannot save '%s' in '%s'\n", fileName, session->storageDir);
        return replyAndLog(session, when, request, REPLY_CANNOT_SAVE);
    }

    /* The client disconnected before sending the whole file: log how much
     * arrived and end the session. */
    char result[MAX_RESULT_LEN];
    snprintf(result, sizeof(result), "-ERR Connection closed (%" PRIu64 "/%" PRIu64 " bytes received)",
             received, fileSize);
    logRequest(session, when, request, result);
    return -1;
}

void handleClient(int connFd, const char *clientAddress, const char *storageDir,
                  const char *logPath){
    Session session;
    initConnection(&session.conn, connFd);
    session.clientAddress = clientAddress;
    session.storageDir = storageDir;
    session.logPath = logPath;

    if (replyAndLog(&session, time(NULL), NULL, REPLY_WELCOME) != 0){
        return;
    }

    char line[MAX_LINE_LEN + 1];
    char request[MAX_LINE_LEN + 3];
    for (;;){
        int length = recvLine(&session.conn, line, sizeof(line));
        time_t when = time(NULL);

        if (length == STREAM_TOO_LONG){
            replyAndLog(&session, when, line, REPLY_COMMAND_TOO_LONG);
            return;
        }
        if (length < 0){
            return; /* the client closed the connection */
        }

        /* Log the request as it is sent on the wire, with its "\r\n". */
        snprintf(request, sizeof(request), "%s\r\n", line);
        if ((size_t)length != strlen(line)){
            /* an embedded '\0' would hide the rest of the line */
            if (replyAndLog(&session, when, request, REPLY_INVALID_COMMAND) != 0){
                return;
            }
            continue;
        }
        if (handleUpload(&session, line, request, when) != 0){
            return;
        }
    }
}
