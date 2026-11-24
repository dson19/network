#ifndef LOG_H
#define LOG_H

#include "conn.h"

/*
 * Log theo kiểu các bài tập (HW4), các trường ngăn cách bằng '$':
 *   [dd/mm/yyyy hh:mm:ss]$ip:port$username$request$code msg
 *   [dd/mm/yyyy hh:mm:ss]$nguồn$nội dung
 * Dòng log được đẩy sang DB writer để ghi ra file.
 */

/* Cố định múi giờ Việt Nam. Gọi 1 lần trước khi tạo luồng. */
void log_set_timezone(void);

/* 1 request của client, kèm mã và msg của response. */
void log_request(const Conn *c, int code, const char *msg);

/* Việc của 1 kết nối: "CONNECT", "DISCONNECT closed"... */
void log_conn(const Conn *c, const char *what);

/* Việc do server tự làm. */
void log_server(const char *fmt, ...);

#endif
