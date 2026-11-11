#ifndef ACCOUNT_H
#define ACCOUNT_H

#define ACCOUNT_FILE "account.txt"

/* Outcomes of findAccount. */
enum {
    ACCOUNT_ACTIVE,
    ACCOUNT_BANNED,
    ACCOUNT_NOT_FOUND
};

/**
 * @function loadAccounts: Read the account file into memory, once, before
 *           the worker threads start. Reading the file on every login would
 *           stop all the clients of a worker while it scans the file.
 *           Each line is "username status" (status 1 = active, 0 = banned),
 *           with LF or CRLF line endings; malformed lines are skipped. If a
 *           username appears twice, the first line wins.
 *
 * @param path: Path of the account file.
 *
 * @return: number of accounts loaded (>= 0) if success.
 *          -1 if the file cannot be read or memory runs out.
 */
long loadAccounts(const char *path);

/**
 * @function findAccount: Look up a user in the loaded accounts. The accounts
 *           do not change after loadAccounts, so any thread may call this
 *           at any time without a lock.
 *
 * @param username: The username to look for (exact, case-sensitive match).
 *
 * @return: ACCOUNT_ACTIVE or ACCOUNT_BANNED if the user is found.
 *          ACCOUNT_NOT_FOUND otherwise (always for "").
 */
int findAccount(const char *username);

#endif
