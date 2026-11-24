#ifndef SESSION_H
#define SESSION_H

#include "cJSON.h"
#include "conn.h"
#include "pool.h"

/* Các lệnh tài khoản và phiên. */
void cmd_ping(Conn *c, const cJSON *req);
void cmd_register(Conn *c, const cJSON *req);
void cmd_login(Conn *c, const cJSON *req);
void cmd_logout(Conn *c, const cJSON *req);
void cmd_whoami(Conn *c, const cJSON *req);
void cmd_change_password(Conn *c, const cJSON *req);

/* Kết quả từ thread pool / DB writer. Hàm giải phóng job hoặc gửi tiếp. */
void session_on_result(Job *job);

/* Kết nối sắp đóng: hủy phiên nếu có. */
void session_on_close(Conn *c);

/* Gửi FORCE_LOGOUT với reason và hủy phiên (không đóng kết nối). */
void session_force_logout(Conn *c, const char *reason);

/* Nạp tài khoản từ DB lên RAM lúc khởi động. */
int  session_load_users(const char *db_path);

#endif
