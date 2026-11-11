#ifndef ACCOUNT_H
#define ACCOUNT_H

#define ACCOUNT_FILE "account.txt"

/* Outcomes of findAccount. */
enum {
    ACCOUNT_ACTIVE,
    ACCOUNT_BANNED,
    ACCOUNT_NOT_FOUND,
    ACCOUNT_FILE_ERROR
};

/**
 * @function checkAccountFile: Check that the account file can be read.
 *
 * @param path: Path of the account file.
 *
 * @return: 0 if the file can be opened for reading.
 *          -1 otherwise.
 */
int checkAccountFile(const char *path);

/**
 * @function findAccount: Look up a user in the account file. The file is
 *           read again on every call, so changes to it apply to the next
 *           login. Each line is "username status" (status 1 = active,
 *           0 = banned), with LF or CRLF line endings; malformed lines are
 *           skipped. The first line with a matching username wins.
 *
 * @param path: Path of the account file.
 * @param username: The username to look for (exact, case-sensitive match).
 *
 * @return: ACCOUNT_ACTIVE or ACCOUNT_BANNED if the user is found.
 *          ACCOUNT_NOT_FOUND if no line matches (always for "").
 *          ACCOUNT_FILE_ERROR if the file cannot be read.
 */
int findAccount(const char *path, const char *username);

#endif
