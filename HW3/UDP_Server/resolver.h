#ifndef RESOLVER_H
#define RESOLVER_H

#include "util.h"

/**
 * @function forwardLookup: Resolve a domain name to its IPv4 addresses.
 *
 * @param domain: A valid domain name (see isValidDomain).
 * @param result: Output list, filled with dotted-decimal IPv4 strings
 *        without duplicates. Must be initialized by the caller.
 *
 * @return: number of addresses found (> 0) if success.
 *          0 if the domain cannot be resolved.
 *          -1 on invalid arguments or memory allocation failure.
 */
int forwardLookup(const char *domain, StringList *result);

/**
 * @function reverseLookup: Resolve an IPv4 address to every domain name
 *           mapped to it: every PTR record from DNS, or the entry from
 *           local sources (e.g. /etc/hosts) if DNS has none.
 *
 * @param ip: A valid dotted-decimal IPv4 address (see isValidIPv4).
 * @param result: Output list, filled with domain names without duplicates.
 *        Must be initialized by the caller.
 *
 * @return: number of names found (> 0) if success.
 *          0 if no name can be resolved.
 *          -1 on invalid arguments or memory allocation failure.
 */
int reverseLookup(const char *ip, StringList *result);

#endif
