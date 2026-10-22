#include "session.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "account.h"
#include "protocol.h"
#include "stream.h"

/* Pause between replies to requests that arrived together in one packet. */
#define PIPELINE_REPLY_GAP_NS (20 * 1000000L)

/* State of one client session, checked on every request. */
typedef struct {
    Connection conn;
    const char *clientAddress;
    const char *accountPath;
    bool loggedIn;
    char username[MAX_LINE_LEN + 1];
} Session;

/* Handle "USER username". Returns the reply code. */
static const char *handleLogin(Session *session, const char *username){
    if (session->loggedIn){
        return REPLY_ALREADY_LOGGED_IN;
    }

    switch (findAccount(session->accountPath, username)){
        case ACCOUNT_ACTIVE:
            session->loggedIn = true;
            snprintf(session->username, sizeof(session->username), "%s", username);
            return REPLY_LOGIN_OK;
        case ACCOUNT_BANNED:
            return REPLY_ACCOUNT_BANNED;
        case ACCOUNT_FILE_ERROR:
            fprintf(stderr, "Error: cannot read account file '%s'\n", session->accountPath);
            return REPLY_ACCOUNT_NOT_FOUND;
        default:
            return REPLY_ACCOUNT_NOT_FOUND;
    }
}

/* Handle "POST article" (the article is shown in the request log). Returns the reply code. */
static const char *handlePost(const Session *session){
    if (!session->loggedIn){
        return REPLY_NOT_LOGGED_IN;
    }
    return REPLY_POST_OK;
}

/* Handle "BYE". Returns the reply code. */
static const char *handleLogout(Session *session){
    if (!session->loggedIn){
        return REPLY_NOT_LOGGED_IN;
    }
    session->loggedIn = false;
    session->username[0] = '\0';
    return REPLY_LOGOUT_OK;
}

/* Process one complete request line. Returns the reply code. */
static const char *processRequest(Session *session, const char *line){
    const char *argument = NULL;
    switch (parseRequest(line, &argument)){
        case REQUEST_LOGIN:
            return handleLogin(session, argument);
        case REQUEST_POST:
            return handlePost(session);
        case REQUEST_LOGOUT:
            return handleLogout(session);
        default:
            return REPLY_UNKNOWN_REQUEST;
    }
}

void handleClient(int connFd, const char *clientAddress, const char *accountPath){
    Session session;
    initConnection(&session.conn, connFd);
    session.clientAddress = clientAddress;
    session.accountPath = accountPath;
    session.loggedIn = false;
    session.username[0] = '\0';

    if (sendMessage(connFd, REPLY_WELCOME) != 0){
        return;
    }

    char line[MAX_LINE_LEN + 1];
    for (;;){
        int length = recvLine(&session.conn, line, sizeof(line));
        const char *reply;

        if (length == STREAM_TOO_LONG){
            reply = REPLY_UNKNOWN_REQUEST;
            printf("[%s] (request longer than %d bytes) -> %s\n", clientAddress, MAX_LINE_LEN, reply);
        } else if (length < 0){
            return; /* the client closed the connection */
        } else {
            /* an embedded '\0' would hide the rest of the line */
            reply = ((size_t)length == strlen(line)) ? processRequest(&session, line)
                                                      : REPLY_UNKNOWN_REQUEST;
            printf("[%s] %s -> %s\n", clientAddress, line, reply);
        }

        if (sendMessage(connFd, reply) != 0){
            return;
        }

        /* More requests are already buffered (the client sent several at
         * once): wait a little before the next reply, so that each reply
         * reaches the client as a separate segment. Clients that wait for
         * every reply before sending the next request never pause here. */
        if (session.conn.start != session.conn.end){
            struct timespec gap = { 0, PIPELINE_REPLY_GAP_NS };
            nanosleep(&gap, NULL);
        }
    }
}
