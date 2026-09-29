#ifndef SESSION_H
#define SESSION_H

#include <stdbool.h>

#include "account.h"

/**
 * @function isLoggedIn: Kiểm tra hiện có người dùng nào đang đăng nhập không.
 *
 * @return: true nếu đang có người đăng nhập.
 *          false nếu chưa ai đăng nhập.
 */
bool isLoggedIn(void);

/**
 * @function login: Ghi nhận `account` là người dùng đang đăng nhập.
 *
 * @param account: Con trỏ tới tài khoản vừa đăng nhập thành công
 *        (lấy từ findAccount trong account.h).
 */
void login(const Account *account);

/**
 * @function logout: Xóa trạng thái đăng nhập hiện tại.
 */
void logout(void);

/**
 * @function getCurrentUsername: Lấy username của người đang đăng nhập.
 *
 * @return: Con trỏ tới username nếu đang có người đăng nhập.
 *          NULL nếu chưa ai đăng nhập.
 */
const char *getCurrentUsername(void);

#endif
