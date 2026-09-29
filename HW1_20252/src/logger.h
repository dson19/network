#ifndef LOGGER_H
#define LOGGER_H

#include <stdbool.h>

/**
 * @function writeLog: Ghi 1 dòng log vào file, theo định dạng:
 *           [dd/mm/yyyy hh:mm:ss] $ function $ value $ +OK/-ERR
 *
 * @param logPath: Đường dẫn file log (VD "log_20235994.txt").
 * @param function: Số hiệu chức năng được chọn (1-4).
 * @param value: Giá trị người dùng nhập cho chức năng đó
 *        (chuỗi rỗng "" nếu chức năng không có input, VD logout/exit).
 * @param success: true nếu chức năng thực hiện thành công (+OK),
 *        false nếu thất bại (-ERR).
 *
 * @return: 0 nếu ghi log thành công.
 *          1 nếu không mở được file log (lỗi I/O).
 */
int writeLog(const char *logPath, int function, const char *value, bool success);

#endif
