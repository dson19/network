#include "session.h"
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "account.h"

/* Pause between replies to requests that arrived together in one packet. */
#define PIPELINE_REPLY_GAP_MS 20

Session *createSession(int fd, const char *clientAddress){
    Session *session = malloc(sizeof(*session));
    if (session == NULL){
        return NULL;
    }

    session->fd = fd;
    snprintf(session->clientAddress, sizeof(session->clientAddress), "%s", clientAddress);
    session->username[0] = '\0';
    session->loggedIn = false;
    session->in.length = 0;
    session->in.overflowed = false;
    session->out.start = 0;
    session->out.end = 0;
    session->peerClosed = false;
    session->resumeAtMs = 0;
    session->next = NULL;

    queueReply(&session->out, REPLY_WELCOME);
    return session;
}

void destroySession(Session *session){
    close(session->fd);
    free(session);
}

short sessionPollEvents(const Session *session){
    short events = 0;
    if (!session->peerClosed && hasRecvSpace(&session->in)){
        events |= POLLIN;
    }
    if (hasPendingSend(&session->out)){
        events |= POLLOUT;
    }
    return events;
}

int sessionReceive(Session *session){
    if (!hasRecvSpace(&session->in)){
        return 0;
    }

    int status = fillRecvBuffer(session->fd, &session->in);
    if (status == STREAM_CLOSED){
        session->peerClosed = true;
        return 0;
    }
    return (status == 0) ? 0 : -1;
}

/* Handle "USER username". Returns the reply code. */
static const char *handleLogin(Session *session, const char *username){
    if (session->loggedIn){
        return REPLY_ALREADY_LOGGED_IN;
    }

    switch (findAccount(username)){
        case ACCOUNT_ACTIVE:
            session->loggedIn = true;
            snprintf(session->username, sizeof(session->username), "%s", username);
            return REPLY_LOGIN_OK;
        case ACCOUNT_BANNED:
            return REPLY_ACCOUNT_BANNED;
        default:
            return REPLY_ACCOUNT_NOT_FOUND;
    }
}

/* Handle "POST article". Returns the reply code. */
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

int sessionAdvance(Session *session, int64_t nowMs){
    char line[MAX_LINE_LEN + 1];

    for (;;){
        if (flushSendBuffer(session->fd, &session->out) != 0){
            return -1;
        }
        if (hasPendingSend(&session->out)){
            return 0; /* the socket is full: go on when poll() reports POLLOUT */
        }
        if (!session->peerClosed && session->resumeAtMs > nowMs){
            return 0; /* the worker wakes up at resumeAtMs */
        }

        int length = takeLine(&session->in, line);
        if (length == STREAM_INCOMPLETE){
            return session->peerClosed ? -1 : 0;
        }

        const char *reply = REPLY_UNKNOWN_REQUEST;
        /* an embedded '\0' would hide the rest of the line */
        if (length >= 0 && (size_t)length == strlen(line)){
            reply = processRequest(session, line);
        }
        queueReply(&session->out, reply);

        /* More requests are already buffered (the client sent several at
         * once): wait a little before the next reply, so that each reply
         * reaches the client as a separate segment. This only delays the
         * next reply of this client: the worker keeps serving the others. */
        session->resumeAtMs = hasCompleteLine(&session->in) ? nowMs + PIPELINE_REPLY_GAP_MS : 0;
    }
}
