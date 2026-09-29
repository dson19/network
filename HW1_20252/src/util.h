#ifndef UTIL_H
#define UTIL_H

/**
 * @function readLine: Đọc một dòng input từ bàn phím (stdin), loại bỏ
 *           ký tự xuống dòng ở cuối.
 *
 * @return: Con trỏ tới chuỗi vừa đọc được, đã được cấp phát động bằng
 *          malloc, trả về NULL nếu gặp lỗi đọc hoặc EOF (VD người dùng nhấn Ctrl+D).
 */
char *readLine(void);

#endif