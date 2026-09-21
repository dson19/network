#ifndef SESSION_H
#define SESSION_H

#include "account.h"

/* Tracks which account, if any, is currently logged in. There is only ever
 * one active session, matching the single-user command-line program. */
typedef struct {
    const Account *current; /* NULL when nobody is logged in */
} Session;

/* Initializes `session` to the "logged out" state. */
void session_init(Session *session);

/* Returns non-zero if a user is currently logged in. */
int session_is_logged_in(const Session *session);

/* Marks `account` as the logged-in user. */
void session_login(Session *session, const Account *account);

/* Clears the logged-in state. */
void session_logout(Session *session);

/* Returns the current user's username, or NULL if nobody is logged in. */
const char *session_username(const Session *session);

#endif /* SESSION_H */
