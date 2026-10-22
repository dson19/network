#ifndef LOGGER_H
#define LOGGER_H

#include <time.h>

/**
 * @function writeLog: Append one line to the log file:
 *           [dd/mm/yyyy hh:mm:ss]$clientAddress$request$result
 *           or, when there is no request (welcome message):
 *           [dd/mm/yyyy hh:mm:ss]$clientAddress$result
 *           CR and LF are written as the two characters "\r" and "\n";
 *           other control characters and '$' are replaced by '?', so that
 *           every entry is exactly one well-formed line.
 *
 * @param logPath: Path of the log file (e.g. "log_20235994.txt").
 * @param when: Time the request was received.
 * @param clientAddress: Client address as "IPAddress:PortNumber".
 * @param request: The request as received, or NULL if there is none.
 * @param result: The result sent back ("+OK ..." or "-ERR ...").
 *
 * @return: 0 if the line was written.
 *          1 if the log file could not be opened.
 */
int writeLog(const char *logPath, time_t when, const char *clientAddress,
             const char *request, const char *result);

#endif
