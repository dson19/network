#ifndef UTIL_H
#define UTIL_H

/* A dynamically growing list of unique strings. */
typedef struct {
    char **items;
    int count;
} StringList;

/**
 * @function initStringList: Initialize an empty list.
 *
 * @param list: The list to initialize (must not be NULL).
 */
void initStringList(StringList *list);

/**
 * @function addUniqueString: Append a copy of `value` to the list unless
 *           an identical string is already stored.
 *
 * @param list: The list to modify.
 * @param value: The string to add.
 *
 * @return: 0 if the string was added or was already present.
 *          -1 if the arguments are NULL or memory allocation failed.
 */
int addUniqueString(StringList *list, const char *value);

/**
 * @function freeStringList: Release every string and the list storage,
 *           then reset the list to empty.
 *
 * @param list: The list to release.
 */
void freeStringList(StringList *list);

#endif
