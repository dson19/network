#include "resolver.h"
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>

int forwardLookup(const char *domain, StringList *result){
    if (domain == NULL || result == NULL){
        return -1;
    }

    struct hostent *host = gethostbyname(domain);
    if (host == NULL || host->h_addrtype != AF_INET || host->h_addr_list == NULL){
        return 0;
    }

    for (char **addr = host->h_addr_list; *addr != NULL; addr++){
        char text[INET_ADDRSTRLEN];
        if (inet_ntop(AF_INET, *addr, text, sizeof(text)) == NULL){
            continue;
        }
        if (addUniqueString(result, text) != 0){
            return -1;
        }
    }
    return result->count;
}

int reverseLookup(const char *ip, StringList *result){
    if (ip == NULL || result == NULL){
        return -1;
    }

    struct in_addr address;
    if (inet_pton(AF_INET, ip, &address) != 1){
        return -1;
    }

    /* gethostbyaddr returns the first PTR name in h_name and the others in
     * h_aliases; the data is static, so copy it out immediately. */
    struct hostent *host = gethostbyaddr(&address, sizeof(address), AF_INET);
    if (host == NULL){
        return 0;
    }

    if (host->h_name != NULL && addUniqueString(result, host->h_name) != 0){
        return -1;
    }
    if (host->h_aliases != NULL){
        for (char **alias = host->h_aliases; *alias != NULL; alias++){
            if (addUniqueString(result, *alias) != 0){
                return -1;
            }
        }
    }
    return result->count;
}
