#ifndef LINEBUF_H
#define LINEBUF_H

#include <stddef.h>

#include "protocol.h"

/*
 * Buffer nhận của 1 kết nối. TCP là luồng byte: 1 lần recv() có thể chỉ
 * có nửa message, hoặc nhiều message dính nhau. LineBuf gom byte lại và
 * tách ra từng message theo '\n' (bỏ '\r' đứng trước nếu có).
 *
 * Buffer có đúng MAX_LINE_BYTES byte. Đầy mà vẫn chưa thấy '\n' nghĩa là
 * message dài quá giới hạn.
 */
#define LINEBUF_CAP MAX_LINE_BYTES

typedef struct {
    char   data[LINEBUF_CAP];
    size_t len;
} LineBuf;

void linebuf_init(LineBuf *lb);

/* Chỗ trống để recv() ghi thẳng vào; *avail = số byte còn trống. */
char *linebuf_space(LineBuf *lb, size_t *avail);

/* Báo đã ghi thêm n byte vào chỗ trống. */
void linebuf_commit(LineBuf *lb, size_t n);

/*
 * Lấy 1 message hoàn chỉnh ra `out` (cần >= LINEBUF_CAP byte), kết thúc
 * bằng '\0', không kèm \r\n.
 * Trả về  1: có message, *out_len là độ dài
 *         0: chưa đủ 1 message
 *        -1: buffer đầy mà không có '\n' (message quá dài)
 */
int linebuf_next(LineBuf *lb, char *out, size_t out_cap, size_t *out_len);

#endif
