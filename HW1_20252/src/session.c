#include "session.h"

#include <stddef.h>

void session_init(Session *session) {
    session->current = NULL;
}

int session_is_logged_in(const Session *session) {
    return session->current != NULL;
}

void session_login(Session *session, const Account *account) {
    session->current = account;
}

void session_logout(Session *session) {
    session->current = NULL;
}

const char *session_username(const Session *session) {
    return session->current ? session->current->username : NULL;
}
