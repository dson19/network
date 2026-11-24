/* Unit test cho common/linebuf: tách message từ luồng byte TCP. */
#include <stdio.h>
#include <string.h>

#include "linebuf.h"

static int failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);  \
            failures++;                                                      \
        }                                                                    \
    } while (0)

static void feed(LineBuf *lb, const char *s, size_t n)
{
    size_t avail;
    char *p = linebuf_space(lb, &avail);
    if (n > avail)
        n = avail;
    memcpy(p, s, n);
    linebuf_commit(lb, n);
}

static char out[LINEBUF_CAP + 1];
static size_t out_len;

static void test_split_and_join(void)
{
    LineBuf lb;
    linebuf_init(&lb);

    /* nửa message: chưa có gì */
    feed(&lb, "{\"cmd\":\"PI", strlen("{\"cmd\":\"PI"));
    CHECK(linebuf_next(&lb, out, sizeof out, &out_len) == 0);

    /* phần còn lại + message thứ hai dính liền */
    const char *rest = "NG\"}\r\n{\"cmd\":\"X\"}\n";
    feed(&lb, rest, strlen(rest));
    CHECK(linebuf_next(&lb, out, sizeof out, &out_len) == 1);
    CHECK(strcmp(out, "{\"cmd\":\"PING\"}") == 0);
    CHECK(out_len == 14);
    CHECK(linebuf_next(&lb, out, sizeof out, &out_len) == 1);   /* chấp nhận \n đơn */
    CHECK(strcmp(out, "{\"cmd\":\"X\"}") == 0);
    CHECK(linebuf_next(&lb, out, sizeof out, &out_len) == 0);
    CHECK(lb.len == 0);
}

static void test_empty_line(void)
{
    LineBuf lb;
    linebuf_init(&lb);
    feed(&lb, "\r\n", 2);
    CHECK(linebuf_next(&lb, out, sizeof out, &out_len) == 1);
    CHECK(out_len == 0 && out[0] == '\0');
}

static void test_limit(void)
{
    static char big[LINEBUF_CAP];
    LineBuf lb;

    /* đúng 8192 byte tính cả \r\n: hợp lệ */
    memset(big, 'a', sizeof big);
    big[LINEBUF_CAP - 2] = '\r';
    big[LINEBUF_CAP - 1] = '\n';
    linebuf_init(&lb);
    feed(&lb, big, sizeof big);
    CHECK(linebuf_next(&lb, out, sizeof out, &out_len) == 1);
    CHECK(out_len == LINEBUF_CAP - 2);

    /* 8192 byte mà chưa có \n: quá dài */
    memset(big, 'a', sizeof big);
    linebuf_init(&lb);
    feed(&lb, big, sizeof big);
    CHECK(linebuf_next(&lb, out, sizeof out, &out_len) == -1);
}

int main(void)
{
    test_split_and_join();
    test_empty_line();
    test_limit();
    if (failures) {
        fprintf(stderr, "test_linebuf: %d failure(s)\n", failures);
        return 1;
    }
    printf("test_linebuf: all passed\n");
    return 0;
}
