#include "linebuf.h"

#include <string.h>

void linebuf_init(LineBuf *lb)
{
    lb->len = 0;
}

char *linebuf_space(LineBuf *lb, size_t *avail)
{
    *avail = LINEBUF_CAP - lb->len;
    return lb->data + lb->len;
}

void linebuf_commit(LineBuf *lb, size_t n)
{
    lb->len += n;
}

int linebuf_next(LineBuf *lb, char *out, size_t out_cap, size_t *out_len)
{
    char *nl = memchr(lb->data, '\n', lb->len);
    if (nl == NULL)
        return lb->len == LINEBUF_CAP ? -1 : 0;

    size_t consumed = (size_t)(nl - lb->data) + 1;
    size_t n = consumed - 1;
    if (n > 0 && lb->data[n - 1] == '\r')
        n--;
    if (n >= out_cap)              /* không xảy ra khi out_cap >= LINEBUF_CAP */
        n = out_cap - 1;

    memcpy(out, lb->data, n);
    out[n] = '\0';
    *out_len = n;

    lb->len -= consumed;
    memmove(lb->data, lb->data + consumed, lb->len);
    return 1;
}
