#ifndef DISPATCH_H
#define DISPATCH_H

#include <stddef.h>

#include "cJSON.h"
#include "conn.h"

/* Xử lý 1 message đã tách khỏi luồng byte (không kèm \r\n). */
void dispatch_line(Conn *c, const char *line, size_t len);

/*
 * Gửi response cho request đang chờ của kết nối và ghi log request đó.
 * msg = NULL thì dùng msg mặc định của mã. data chỉ được gửi khi code = 1000;
 * hàm luôn giải phóng data.
 */
void respond(Conn *c, int code, const char *msg, cJSON *data);

/* Gửi event (server tự gửi). Hàm luôn giải phóng data. */
void send_event(Conn *c, const char *evt, cJSON *data);

#endif
