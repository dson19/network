#include "cache.h"

#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Bảng băm nối chuỗi: username (chữ thường) -> User. */
#define BUCKETS 4096

static User *table[BUCKETS];

static unsigned hash_name(const char *s)
{
    unsigned h = 5381;                    /* djb2 */
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c >= 'A' && c <= 'Z')
            c = (unsigned char)(c - 'A' + 'a');
        h = h * 33 + c;
    }
    return h % BUCKETS;
}

void cache_init(void)
{
    memset(table, 0, sizeof table);
}

void cache_free(void)
{
    for (int i = 0; i < BUCKETS; i++) {
        User *u = table[i];
        while (u != NULL) {
            User *next = u->next;
            free(u);
            u = next;
        }
        table[i] = NULL;
    }
}

User *cache_find(const char *username)
{
    for (User *u = table[hash_name(username)]; u != NULL; u = u->next)
        if (strcasecmp(u->username, username) == 0)
            return u;
    return NULL;
}

User *cache_add(int64_t id, const char *username, Role role, int locked)
{
    User *u = calloc(1, sizeof *u);
    if (u == NULL)
        return NULL;
    u->id = id;
    strncpy(u->username, username, USERNAME_MAX);
    u->role = role;
    u->locked = locked;
    unsigned b = hash_name(username);
    u->next = table[b];
    table[b] = u;
    return u;
}

void cache_remove(User *u)
{
    User **pp = &table[hash_name(u->username)];
    while (*pp != NULL && *pp != u)
        pp = &(*pp)->next;
    if (*pp == u) {
        *pp = u->next;
        free(u);
    }
}

const char *role_name(Role r)
{
    return r == ROLE_ADMIN ? "ADMIN" : "PLAYER";
}
