#include "registry.h"
#include <pthread.h>
#include <stddef.h>
#include <string.h>

/* Accounts currently logged in, shared by all client threads. Every access
 * to `loggedIn` must hold `registryLock`. */
static LoginEntry *loggedIn = NULL;
static pthread_mutex_t registryLock = PTHREAD_MUTEX_INITIALIZER;

bool claimAccount(LoginEntry *entry, const char *username){
    bool claimed = true;

    pthread_mutex_lock(&registryLock);
    for (const LoginEntry *e = loggedIn; e != NULL; e = e->next){
        if (strcmp(e->username, username) == 0){
            claimed = false;
            break;
        }
    }
    if (claimed){
        entry->username = username;
        entry->next = loggedIn;
        loggedIn = entry;
    }
    pthread_mutex_unlock(&registryLock);

    return claimed;
}

void releaseAccount(LoginEntry *entry){
    pthread_mutex_lock(&registryLock);
    for (LoginEntry **link = &loggedIn; *link != NULL; link = &(*link)->next){
        if (*link == entry){
            *link = entry->next;
            break;
        }
    }
    pthread_mutex_unlock(&registryLock);
}
