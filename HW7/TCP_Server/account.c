#include "account.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WHITESPACE " \t\r\n"
#define BUCKET_COUNT 65536

/* One account in a hash bucket. The username is stored right after the struct. */
typedef struct Account {
    struct Account *next;
    bool active;
    char username[];
} Account;

/* Hash table of the accounts: written by loadAccounts, read-only afterwards. */
static Account *buckets[BUCKET_COUNT];

/* FNV-1a hash of a string, reduced to a bucket index. */
static size_t bucketOf(const char *username){
    uint32_t hash = 2166136261u;
    for (const char *p = username; *p != '\0'; p++){
        hash ^= (unsigned char)*p;
        hash *= 16777619u;
    }
    return hash % BUCKET_COUNT;
}

static const Account *lookup(const char *username){
    for (const Account *a = buckets[bucketOf(username)]; a != NULL; a = a->next){
        if (strcmp(a->username, username) == 0){
            return a;
        }
    }
    return NULL;
}

/* Split an account line "username status" in place. Returns the status
 * (0 or 1) and sets *username, or -1 if the line is malformed. */
static int parseAccountLine(char *line, char **username){
    char *save = NULL;
    char *name = strtok_r(line, WHITESPACE, &save);
    char *status = strtok_r(NULL, WHITESPACE, &save);
    if (name == NULL || status == NULL || strtok_r(NULL, WHITESPACE, &save) != NULL){
        return -1;
    }
    if (strcmp(status, "0") != 0 && strcmp(status, "1") != 0){
        return -1;
    }
    *username = name;
    return status[0] - '0';
}

/* Add an account to the table. Returns 0 if success, -1 if out of memory. */
static int addAccount(const char *username, bool active){
    size_t size = strlen(username) + 1;
    Account *account = malloc(sizeof(*account) + size);
    if (account == NULL){
        return -1;
    }
    memcpy(account->username, username, size);
    account->active = active;

    size_t bucket = bucketOf(username);
    account->next = buckets[bucket];
    buckets[bucket] = account;
    return 0;
}

long loadAccounts(const char *path){
    FILE *file = fopen(path, "r");
    if (file == NULL){
        return -1;
    }

    /* getline grows the buffer as needed: account lines have no length limit. */
    char *line = NULL;
    size_t capacity = 0;
    long count = 0;
    while (getline(&line, &capacity, file) != -1){
        char *name = NULL;
        int status = parseAccountLine(line, &name);
        if (status < 0 || lookup(name) != NULL){
            continue;
        }
        if (addAccount(name, status == 1) != 0){
            count = -1;
            break;
        }
        count++;
    }
    if (ferror(file)){
        count = -1;
    }

    free(line);
    fclose(file);
    return count;
}

int findAccount(const char *username){
    const Account *account = lookup(username);
    if (account == NULL){
        return ACCOUNT_NOT_FOUND;
    }
    return account->active ? ACCOUNT_ACTIVE : ACCOUNT_BANNED;
}
