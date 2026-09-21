#ifndef LOGGER_H
#define LOGGER_H

/* Result code recorded for a request, matching the log format's "+OK"/"-ERR". */
typedef enum {
    LOG_OK,
    LOG_ERR
} LogStatus;

/*
 * Appends one entry to the log file at `log_path`, in the required format:
 *   [dd/mm/yyyy hh:mm:ss] $ <function> $ <value> $ <+OK|-ERR>
 *
 * `function` is the selected menu option (1-4). `value` is the raw input
 * the user provided for that request ("" if the request takes no input,
 * e.g. logout or exit).
 *
 * Returns 0 on success. Returns -1 if the log file could not be opened
 * for writing (the caller is responsible for reporting the I/O error).
 */
int logger_write(const char *log_path, int function, const char *value, LogStatus status);

#endif /* LOGGER_H */
