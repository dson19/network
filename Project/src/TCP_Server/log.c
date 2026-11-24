#include "log.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cache.h"
#include "pool.h"

void log_set_timezone(void)
{
    setenv("TZ", "Asia/Ho_Chi_Minh", 1);
    tzset();
}

typedef struct {
    char  *buf;
    size_t len, cap;
} Str;

static void put(Str *s, const char *p, size_t n)
{
    if (s->len + n + 1 > s->cap) {
        size_t cap = s->cap ? s->cap : 256;
        while (s->len + n + 1 > cap)
            cap *= 2;
        char *nb = realloc(s->buf, cap);
        if (!nb)
            return;
        s->buf = nb;
        s->cap = cap;
    }
    memcpy(s->buf + s->len, p, n);
    s->len += n;
    s->buf[s->len] = '\0';
}

/* Như HW4: \r \n viết thành 2 ký tự; ký tự điều khiển và '$' đổi thành '?'. */
static void put_field(Str *s, const char *text)
{
    for (const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if (*p == '\r')
            put(s, "\\r", 2);
        else if (*p == '\n')
            put(s, "\\n", 2);
        else if (*p < 0x20 || *p == 0x7f || *p == '$')
            put(s, "?", 1);
        else
            put(s, (const char *)p, 1);
    }
}

static void put_time(Str *s)
{
    char ts[32];
    time_t now = time(NULL);
    struct tm tm;
    localtime_r(&now, &tm);
    strftime(ts, sizeof ts, "[%d/%m/%Y %H:%M:%S]", &tm);
    put(s, ts, strlen(ts));
}

static void submit(Str *s)
{
    if (!s->buf)
        return;
    put(s, "\n", 1);
    Job *j = pool_running() ? job_new(JOB_W_LOG) : NULL;
    if (!j) {
        free(s->buf);
        return;
    }
    j->text = s->buf;                      /* job giữ buffer, writer giải phóng */
    pool_submit_write(j);
}

void log_request(const Conn *c, int code, const char *msg)
{
    Str s = {0};
    char res[160];
    put_time(&s);
    put(&s, "$", 1);
    put_field(&s, c->peer);
    put(&s, "$", 1);
    put_field(&s, c->user ? c->user->username : "-");
    put(&s, "$", 1);
    put_field(&s, c->req_text ? c->req_text : "");
    put(&s, "$", 1);
    snprintf(res, sizeof res, "%d %s", code, msg);
    put_field(&s, res);
    submit(&s);
}

void log_conn(const Conn *c, const char *what)
{
    Str s = {0};
    put_time(&s);
    put(&s, "$", 1);
    put_field(&s, c->peer);
    put(&s, "$", 1);
    put_field(&s, what);
    submit(&s);
}

void log_server(const char *fmt, ...)
{
    char text[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);

    Str s = {0};
    put_time(&s);
    put(&s, "$SERVER$", 8);
    put_field(&s, text);
    submit(&s);
}
