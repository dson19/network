#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>

#include <sqlite3.h>

#include "auth.h"

/*
 * Toàn bộ SQL của server nằm ở đây. Các hàm chạy trên luồng pool hoặc
 * DB writer (riêng lúc khởi động và --create-admin thì chạy trên luồng chính).
 * Mỗi luồng giữ 1 kết nối sqlite3 riêng.
 */
enum { STORAGE_OK = 0, STORAGE_CONSTRAINT = 1, STORAGE_NOT_FOUND = 2, STORAGE_ERROR = -1 };

/* Mở DB, bật pragma, tạo lược đồ nếu DB trống. */
int  storage_open(const char *path, sqlite3 **out);
void storage_close(sqlite3 *db);

typedef void (*storage_user_cb)(int64_t id, const char *username, const char *role,
                                int locked, void *arg);
int storage_load_users(sqlite3 *db, storage_user_cb cb, void *arg);

int storage_user_find(sqlite3 *db, const char *username, int64_t *out_id);
int storage_user_get_credentials(sqlite3 *db, int64_t user_id, uint8_t salt[SALT_LEN],
                                 uint8_t hash[HASH_LEN], int *iterations);
int storage_user_create(sqlite3 *db, const char *username, const uint8_t hash[HASH_LEN],
                        const uint8_t salt[SALT_LEN], int iterations, const char *role,
                        int64_t *out_id);
int storage_user_set_password(sqlite3 *db, int64_t user_id, const uint8_t hash[HASH_LEN],
                              const uint8_t salt[SALT_LEN], int iterations);
int storage_user_set_role(sqlite3 *db, int64_t user_id, const char *role);
int storage_user_touch_login(sqlite3 *db, int64_t user_id, int64_t when);

#endif
