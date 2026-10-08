#ifndef HANDLER_H
#define HANDLER_H

#include <stddef.h>

/**
 * @function handleQuery: Resolve one request and build the response message.
 *           - valid IPv4 address  -> reverse lookup, "+name1 name2 ..."
 *           - valid domain name   -> forward lookup, "+ip1 ip2 ..."
 *           - anything else, or nothing found -> "-Not found information"
 *           - internal failure (e.g. out of memory) -> "-Server error"
 *
 * @param request: The request text, '\0'-terminated (must not be NULL).
 * @param response: Output buffer for the response message.
 * @param size: Size of the output buffer in bytes.
 */
void handleQuery(const char *request, char *response, size_t size);

#endif
