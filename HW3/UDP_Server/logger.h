#ifndef LOGGER_H
#define LOGGER_H

/**
 * @function writeLog: Append one line to the log file:
 *           [dd/mm/yyyy hh:mm:ss]$request$response
 *           Non-printable or non-ASCII bytes and '$' inside the texts are
 *           replaced by '?'
 *           so that every request produces exactly one well-formed line.
 *
 * @param logPath: Path of the log file (e.g. "log_20235994.txt").
 * @param request: The request text as received.
 * @param response: The response message sent back ("+..." or "-...").
 *
 * @return: 0 if the line was written.
 *          1 if the log file could not be opened.
 */
int writeLog(const char *logPath, const char *request, const char *response);

#endif
