#ifndef REGISTRY_H
#define REGISTRY_H

#include <stdbool.h>

/* One entry of the list of logged-in accounts. Each session owns its entry
 * (no allocation is needed), and the username it points to must stay valid
 * while the entry is registered. */
typedef struct LoginEntry {
    const char *username;
    struct LoginEntry *next;
} LoginEntry;

/**
 * @function claimAccount: Register `entry` as the session logged in to
 *           `username`, unless another session already holds that account.
 *           The check and the registration are done under one lock, so when
 *           several threads log in to the same account at the same time,
 *           exactly one of them succeeds.
 *
 * @param entry: The session's entry, not registered yet.
 * @param username: The account name; must outlive the registration.
 *
 * @return: true if the account was free and is now held by `entry`.
 *          false if another session is logged in to this account.
 */
bool claimAccount(LoginEntry *entry, const char *username);

/**
 * @function releaseAccount: Unregister `entry`, so that the account can be
 *           logged in to again. Does nothing if `entry` is not registered.
 *
 * @param entry: An entry previously passed to claimAccount.
 */
void releaseAccount(LoginEntry *entry);

#endif
