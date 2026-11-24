#ifndef CONN_H
#define CONN_H

#include <stddef.h>
#include <stdint.h>

#include "config.h"
#include "linebuf.h"

struct User;

typedef enum {
    CS_CONNECTED,       /* đã kết nối, chưa đăng nhập */
    CS_AUTH_PENDING,    /* đang chờ thread pool kiểm tra mật khẩu */
    CS_LOGGED_IN,       /* đã đăng nhập */
    CS_CLOSING          /* đã đóng, chờ giải phóng cuối vòng lặp */
} ConnState;

typedef struct Conn {
    int          fd;
    uint64_t     id;            /* tăng dần, không dùng lại (fd thì có thể bị cấp lại) */
    ConnState    state;
    char         peer[48];      /* ip:port */

    LineBuf      in;            /* buffer nhận */
    char        *out;           /* buffer gửi: out[out_off .. out_len) chưa gửi */
    size_t       out_len, out_off, out_cap;
    int          want_write;    /* đang theo dõi EPOLLOUT */

    struct User *user;          /* phiên: tài khoản đang đăng nhập, NULL nếu chưa */
    int          busy;          /* đang chờ pool; request sau phải xếp hàng */

    char        *req_text;      /* request hiện tại (đã che mật khẩu) để ghi log */
    int          req_log;       /* 0: không ghi log request này (PING) */

    struct Conn *next_dead;
} Conn;

const Config *server_config(void);

/* Thêm 1 message (chưa có \r\n) vào buffer gửi và thử gửi ngay. */
void  conn_send_line(Conn *c, const char *line);

/* Đóng kết nối, hủy phiên nếu có. Bộ nhớ được giải phóng cuối vòng lặp. */
void  conn_close(Conn *c, const char *reason);

/* Bật/tắt trạng thái chờ pool. Tắt thì xử lý tiếp các request đang xếp hàng. */
void  conn_set_busy(Conn *c, int busy);

/* Kết nối có fd và id này, NULL nếu đã đóng. */
Conn *conn_find(int fd, uint64_t id);

/* Chạy server tới khi nhận SIGINT/SIGTERM. port_file có thể NULL. */
int   loop_run(const Config *cfg, const char *port_file);

#endif
