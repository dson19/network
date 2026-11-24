#ifndef AUTH_H
#define AUTH_H

#include <stdint.h>

#define SALT_LEN 16
#define HASH_LEN 32

/* PBKDF2-HMAC-SHA256. Trả về 0 nếu được. */
int  auth_hash(const char *password, const uint8_t salt[SALT_LEN], int iterations,
               uint8_t out[HASH_LEN]);

/* Salt ngẫu nhiên từ OpenSSL. Trả về 0 nếu được. */
int  auth_new_salt(uint8_t salt[SALT_LEN]);

/* So sánh 2 hash trong thời gian hằng: 1 nếu bằng nhau. */
int  auth_equal(const uint8_t a[HASH_LEN], const uint8_t b[HASH_LEN]);

/* Xóa bí mật khỏi bộ nhớ (không bị trình biên dịch bỏ qua). */
void auth_wipe(void *p, unsigned long n);

#endif
