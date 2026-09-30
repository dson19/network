#ifndef VALIDATOR_H
#define VALIDATOR_H

#include <stdbool.h>

#define MAX_DOMAIN_LEN 253
#define MAX_LABEL_LEN 63

/**
 * @function isValidIPv4: Check that a string is a strict dotted-decimal
 *           IPv4 address (exactly four decimal parts, each 0-255).
 *
 * @param text: The string to check.
 *
 * @return: true if `text` is a valid IPv4 address.
 *          false otherwise (including NULL, "1.2.3", "259.12.34.12").
 */
bool isValidIPv4(const char *text);

/**
 * @function isValidDomain: Check that a string is a syntactically valid
 *           host name:
 *           - not NULL/empty, at most 253 characters (one trailing dot allowed);
 *           - labels separated by '.', each 1-63 characters;
 *           - labels contain only letters, digits and '-', and do not
 *             start or end with '-';
 *           - the last label (TLD) is not purely numeric, so numeric
 *             shorthand such as "1.2.3" or "123" is rejected instead of
 *             being expanded to an IP address by the resolver.
 *
 * @param text: The string to check.
 *
 * @return: true if `text` is a valid domain name.
 *          false otherwise.
 */
bool isValidDomain(const char *text);

#endif
