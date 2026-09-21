#ifndef UTIL_H
#define UTIL_H

/*
 * Reads one line from stdin into a newly heap-allocated, NUL-terminated
 * string with the trailing newline (and '\r', if present) stripped.
 * The caller owns the returned string and must free() it.
 *
 * Using a dynamically grown buffer (instead of a fixed-size char array)
 * means input of any length is accepted safely, with no risk of overflow.
 *
 * Returns NULL on EOF or if reading/allocating fails.
 */
char *read_line(void);

#endif /* UTIL_H */
