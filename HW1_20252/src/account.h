#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <stddef.h>

/* One entry loaded from account.txt: a username and its active/banned status. */
typedef struct {
    char *username; /* heap-allocated; owned by the AccountList it lives in */
    int status;      /* 1 = active, 0 = banned/locked */
} Account;

/* Dynamic array of accounts, grown as needed while reading account.txt. */
typedef struct {
    Account *items;
    size_t count;
    size_t capacity;
} AccountList;

/*
 * Reads every "username status" line from the file at `path` into `list`.
 * Each line is read with getline(), so a username of any length is
 * supported without truncation or buffer overflow.
 *
 * Lines that do not match the "username status" format are skipped.
 *
 * Returns 0 on success. Returns -1 if the file could not be opened
 * (the caller is responsible for reporting the I/O error).
 */
int account_load(const char *path, AccountList *list);

/*
 * Looks up an account by exact username match.
 * Returns a pointer owned by `list`, or NULL if no account matches.
 */
Account *account_find(AccountList *list, const char *username);

/* Releases all memory owned by `list` (every username plus the array itself). */
void account_free(AccountList *list);

#endif /* ACCOUNT_H */
