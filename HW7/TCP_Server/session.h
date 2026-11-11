#ifndef SESSION_H
#define SESSION_H

#include <arpa/inet.h>
#include <stdbool.h>
#include <stdint.h>

#include "protocol.h"
#include "stream.h"

#define CLIENT_ADDRESS_LEN (INET_ADDRSTRLEN + 8)

/* Everything the server keeps about one client. A session belongs to one
 * worker thread at a time, so no field needs a lock. */
typedef struct Session {
    int fd;                                 /* socket descriptor on the server */
    char clientAddress[CLIENT_ADDRESS_LEN]; /* "IPAddress:PortNumber" */
    char username[MAX_LINE_LEN + 1];        /* account logged in at this client */
    bool loggedIn;                          /* login state, checked on every request */

    RecvBuffer in;
    SendBuffer out;
    bool peerClosed;     /* the client will not send anything more */
    int64_t resumeAtMs;  /* do not answer the next request before this time; 0 = no pause */

    struct Session *next; /* link in the queue of new sessions of a worker */
} Session;

/**
 * @function createSession: Allocate a session for a new connection and queue
 *           the welcome code.
 *
 * @param fd: The connected non-blocking socket; the session now owns it.
 * @param clientAddress: Client address as "IPAddress:PortNumber".
 *
 * @return: the new session.
 *          NULL if memory runs out (`fd` is left open).
 */
Session *createSession(int fd, const char *clientAddress);

/**
 * @function destroySession: Close the connection and free the session.
 *
 * @param session: The session to destroy.
 */
void destroySession(Session *session);

/**
 * @function sessionPollEvents: Choose what poll() must watch for this
 *           session: POLLIN while bytes can be stored, POLLOUT while a reply
 *           is waiting for space in the socket. A client that does not read
 *           its replies thus stops being read, and uses no more memory.
 *
 * @param session: The session.
 *
 * @return: the events mask for struct pollfd.
 */
short sessionPollEvents(const Session *session);

/**
 * @function sessionReceive: Read the bytes poll() has announced.
 *
 * @param session: The session.
 *
 * @return: 0 if the session goes on.
 *          -1 if the connection failed and the session must be destroyed.
 */
int sessionReceive(Session *session);

/**
 * @function sessionAdvance: Do all the work that is possible right now,
 *           without ever waiting: send the pending reply, then answer the
 *           complete requests that are buffered, one reply at a time.
 *
 * @param session: The session.
 * @param nowMs: Current time in milliseconds (monotonic clock).
 *
 * @return: 0 if the session goes on.
 *          -1 if the session is over and must be destroyed.
 */
int sessionAdvance(Session *session, int64_t nowMs);

#endif
