#ifndef ACCOUNT_H
#define ACCOUNT_H

#define MAX_ACCOUNTS 20000
#define MAX_USERNAME_LEN 10000

/* Một tài khoản: username và trạng thái (1 = active, 0 = banned/locked). */
typedef struct {
    char username[MAX_USERNAME_LEN];
    int status;
} Account;

/**
 * @function loadAccounts: Đọc toàn bộ tài khoản từ file vào danh sách
 *           nội bộ của module.
 *
 * @param path: Đường dẫn tới file chứa danh sách tài khoản (account.txt).
 *
 * @return: số account nếu success.
 *          -1 nếu không được.
 */
int loadAccounts(const char *path);

/**
 * @function findAccount: Tìm tài khoản theo username (so khớp chính xác).
 *
 * @param username: Tên tài khoản cần tìm.
 *
 * @return: Con trỏ tới Account tương ứng nếu tìm thấy.
 *          NULL nếu không có tài khoản nào khớp.
 */
Account *findAccount(const char *username);

#endif
