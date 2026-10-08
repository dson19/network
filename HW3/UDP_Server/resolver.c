#include "resolver.h"
#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <netdb.h>
#include <resolv.h>
#include <stdio.h>

#define DNS_ANSWER_BUFFER_SIZE 4096

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

/* Write the PTR query name of `address`, e.g. 1.2.3.4 -> "4.3.2.1.in-addr.arpa". */
static void buildReverseName(const struct in_addr *address, char *name, size_t size){
    const unsigned char *bytes = (const unsigned char *)&address->s_addr;
    snprintf(name, size, "%u.%u.%u.%u.in-addr.arpa", bytes[3], bytes[2], bytes[1], bytes[0]);
}

/* Query DNS for PTR records and add every target name to `result`.
 * Returns 0 on success (even if there is no answer), -1 on allocation failure. */
static int collectPtrRecords(const char *reverseName, StringList *result){
    unsigned char answer[DNS_ANSWER_BUFFER_SIZE];
    int length = res_query(reverseName, ns_c_in, ns_t_ptr, answer, sizeof(answer));
    if (length <= 0){
        return 0;
    }

    ns_msg message;
    if (ns_initparse(answer, length, &message) < 0){
        return 0;
    }

    int answerCount = ns_msg_count(message, ns_s_an);
    for (int i = 0; i < answerCount; i++){
        ns_rr record;
        if (ns_parserr(&message, ns_s_an, i, &record) < 0 || ns_rr_type(record) != ns_t_ptr){
            continue;
        }

        char name[NS_MAXDNAME];
        if (ns_name_uncompress(ns_msg_base(message), ns_msg_end(message),
                               ns_rr_rdata(record), name, sizeof(name)) < 0){
            continue;
        }
        if (addUniqueString(result, name) != 0){
            return -1;
        }
    }
    return 0;
}

/* Fallback for addresses known only through local sources such as /etc/hosts:
 * add the official name and all aliases returned by gethostbyaddr.
 * Returns 0 on success (even if nothing is found), -1 on allocation failure. */
static int collectHostEntry(const struct in_addr *address, StringList *result){
    struct hostent *host = gethostbyaddr(address, sizeof(*address), AF_INET);
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
    return 0;
}

int reverseLookup(const char *ip, StringList *result){
    if (ip == NULL || result == NULL){
        return -1;
    }

    struct in_addr address;
    if (inet_pton(AF_INET, ip, &address) != 1){
        return -1;
    }

    char reverseName[NS_MAXDNAME];
    buildReverseName(&address, reverseName, sizeof(reverseName));

    /* gethostbyaddr alone keeps only the first PTR record of a DNS answer,
     * so read every PTR record directly from the DNS response. */
    if (collectPtrRecords(reverseName, result) != 0){
        return -1;
    }
    if (result->count == 0 && collectHostEntry(&address, result) != 0){
        return -1;
    }
    return result->count;
}
