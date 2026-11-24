#ifndef PROTOCOL_H
#define PROTOCOL_H

/* Giới hạn của giao thức (PROTOCOL.md mục 2.1). */
#define MAX_LINE_BYTES 8192      /* 1 message, tính cả \r\n */

#define USERNAME_MIN 3
#define USERNAME_MAX 20
#define PASSWORD_MIN 6
#define PASSWORD_MAX 64

/* Mã kết quả (PROTOCOL.md mục 3). Mới khai báo những mã đã dùng tới. */
enum {
    RC_OK                 = 1000,

    RC_BAD_FORMAT         = 2001,
    RC_UNKNOWN_CMD        = 2002,
    RC_BAD_PARAM          = 2003,
    RC_MSG_TOO_LONG       = 2004,

    RC_NOT_LOGGED_IN      = 3001,
    RC_ALREADY_LOGGED_IN  = 3002,
    RC_WRONG_CREDENTIALS  = 3003,
    RC_ACCOUNT_LOCKED     = 3004,
    RC_USERNAME_TAKEN     = 3005,
    RC_INVALID_USERNAME   = 3006,
    RC_INVALID_PASSWORD   = 3007,

    RC_INTERNAL_ERROR     = 9001,
    RC_SERVER_BUSY        = 9002
};

/* msg mặc định của từng mã; "Error" nếu mã chưa có trong bảng. */
const char *proto_default_msg(int code);

/* 3-20 ký tự a-z A-Z 0-9 _ */
int proto_valid_username(const char *s);

/* 6-64 byte */
int proto_valid_password(const char *s);

#endif
