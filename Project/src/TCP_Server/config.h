#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

typedef struct {
    char   bind_address[64];
    int    port;
    size_t send_buffer_max;     /* byte chờ gửi tối đa của 1 kết nối */
    int    read_workers;        /* số luồng thread pool */
    int    job_queue_max;       /* hàng đợi pool đầy thì trả 9002 */
    char   db_path[256];
    char   log_path[256];
    int    pbkdf2_iterations;
} Config;

void config_defaults(Config *cfg);

/*
 * Đọc file "khóa = giá trị", dòng bắt đầu bằng # là chú thích.
 * must_exist = 0: file không có thì giữ mặc định.
 * Trả về 0 nếu được, -1 nếu lỗi (đã in lỗi kèm số dòng ra stderr).
 */
int config_load(Config *cfg, const char *path, int must_exist);

#endif
