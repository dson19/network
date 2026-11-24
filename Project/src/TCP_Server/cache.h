#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>

#include "protocol.h"

struct Conn;

typedef enum { ROLE_PLAYER, ROLE_ADMIN } Role;

/*
 * Tài khoản nạp từ DB lên RAM, tồn tại cả khi offline.
 * Không giữ hash mật khẩu: chỉ đọc từ DB khi cần kiểm tra.
 */
typedef struct User {
    int64_t       id;                     /* 0 khi đang chờ DB cấp (REGISTER) */
    char          username[USERNAME_MAX + 1];
    Role          role;
    int           locked;
    int           pending;                /* 1: tên đang được giữ chỗ cho REGISTER */
    struct Conn  *conn;                   /* phiên hiện tại, NULL = offline */
    struct User  *next;                   /* nối chuỗi trong bảng băm */
} User;

void  cache_init(void);
void  cache_free(void);

/* Tìm theo tên, không phân biệt hoa thường. */
User *cache_find(const char *username);

/* Thêm tài khoản; trả về NULL nếu hết bộ nhớ. */
User *cache_add(int64_t id, const char *username, Role role, int locked);

void  cache_remove(User *u);

const char *role_name(Role r);

#endif
