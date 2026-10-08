#include "util.h"
#include <stdlib.h>
#include <string.h>

void initStringList(StringList *list){
    list->items = NULL;
    list->count = 0;
}

int addUniqueString(StringList *list, const char *value){
    if (list == NULL || value == NULL){
        return -1;
    }

    for (int i = 0; i < list->count; i++){
        if (strcmp(list->items[i], value) == 0){
            return 0;
        }
    }

    char *copy = malloc(strlen(value) + 1);
    if (copy == NULL){
        return -1;
    }
    strcpy(copy, value);

    char **grown = realloc(list->items, (size_t)(list->count + 1) * sizeof(char *));
    if (grown == NULL){
        free(copy);
        return -1;
    }

    list->items = grown;
    list->items[list->count++] = copy;
    return 0;
}

void freeStringList(StringList *list){
    if (list == NULL){
        return;
    }
    for (int i = 0; i < list->count; i++){
        free(list->items[i]);
    }
    free(list->items);
    initStringList(list);
}
