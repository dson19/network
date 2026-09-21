/* Needed for getline(), which is a POSIX extension, not plain C11. */
#define _POSIX_C_SOURCE 200809L

#include "account.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_CAPACITY 16

/* Strips a trailing '\n' and/or '\r' left by getline() (account.txt uses
 * Windows-style CRLF line endings). */
static void strip_newline(char *line) {
    size_t len = strlen(line);
    while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
        line[--len] = '\0';
    }
}

/* Appends one account to the list, growing the backing array (doubling
 * its capacity) whenever it is full. Takes ownership of `username`. */
static void account_list_push(AccountList *list, char *username, int status) {
    if (list->count == list->capacity) {
        size_t new_capacity = (list->capacity == 0) ? INITIAL_CAPACITY : list->capacity * 2;
        Account *resized = realloc(list->items, new_capacity * sizeof(Account));
        if (!resized) {
            fprintf(stderr, "account: out of memory while loading accounts\n");
            exit(EXIT_FAILURE);
        }
        list->items = resized;
        list->capacity = new_capacity;
    }

    list->items[list->count].username = username;
    list->items[list->count].status = status;
    list->count++;
}

int account_load(const char *path, AccountList *list) {
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;

    FILE *fp = fopen(path, "r");
    if (!fp) {
        return -1;
    }

    char *line = NULL;
    size_t buf_size = 0;
    ssize_t line_len;

    while ((line_len = getline(&line, &buf_size, fp)) != -1) {
        strip_newline(line);
        if (line[0] == '\0') {
            continue; /* skip blank lines */
        }

        /* `username` is allocated as large as the whole line, which is
         * always enough to hold the username field, no matter how long
         * it is (the sample account.txt contains a ~300-character one). */
        char *username = malloc((size_t)line_len + 1);
        if (!username) {
            fprintf(stderr, "account: out of memory while loading accounts\n");
            exit(EXIT_FAILURE);
        }

        int status;
        if (sscanf(line, "%s %d", username, &status) != 2) {
            free(username); /* malformed line: not "username status" */
            continue;
        }

        account_list_push(list, username, status);
    }

    free(line);
    fclose(fp);
    return 0;
}

Account *account_find(AccountList *list, const char *username) {
    for (size_t i = 0; i < list->count; i++) {
        if (strcmp(list->items[i].username, username) == 0) {
            return &list->items[i];
        }
    }
    return NULL;
}

void account_free(AccountList *list) {
    for (size_t i = 0; i < list->count; i++) {
        free(list->items[i].username);
    }
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->capacity = 0;
}
