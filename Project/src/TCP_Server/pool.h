#ifndef POOL_H
#define POOL_H

#include <stdint.h>

#include "auth.h"
#include "config.h"
#include "protocol.h"

/*
 * Thread pool (đọc DB, băm mật khẩu) + 1 luồng DB writer (ghi DB, ghi log).
 * Luồng chính đẩy Job vào hàng đợi; Job xong được đẩy sang hàng đợi kết quả
 * và luồng chính được đánh thức qua eventfd.
 */
typedef enum {
    /* chạy trên thread pool */
    JOB_VERIFY_LOGIN,     /* in: user_id (0 = không tồn tại), pw      out: ok */
    JOB_HASH_NEW,         /* in: pw, iterations                         out: salt, hash */
    JOB_CHANGE_PW,        /* in: user_id, pw (cũ), pw2 (mới), iterations out: ok, salt, hash */
    /* chạy trên DB writer */
    JOB_W_INSERT_USER,    /* in: username, salt, hash, iterations, role out: out_id */
    JOB_W_SET_PASSWORD,   /* in: user_id, salt, hash, iterations */
    JOB_W_TOUCH_LOGIN,    /* in: user_id, when             (không trả kết quả) */
    JOB_W_LOG             /* in: text                      (không trả kết quả) */
} JobType;

enum { JOB_STATUS_OK = 0, JOB_STATUS_CONSTRAINT = 1, JOB_STATUS_ERROR = -1 };

typedef struct Job {
    JobType  type;
    int      fd;              /* kết nối đã gửi request */
    uint64_t conn_id;         /* để nhận ra fd đã bị cấp lại cho kết nối khác */

    int64_t  user_id;
    char     username[USERNAME_MAX + 1];
    char     role[8];
    char     pw[PASSWORD_MAX + 1];
    char     pw2[PASSWORD_MAX + 1];
    int      iterations;
    uint8_t  salt[SALT_LEN];
    uint8_t  hash[HASH_LEN];
    int64_t  when;
    char    *text;            /* dòng log, đã có '\n' */

    int      status;          /* JOB_STATUS_* */
    int      ok;              /* kết quả so sánh mật khẩu */
    int64_t  out_id;

    struct Job *next;
} Job;

/* Khởi động các luồng. *event_fd: fd để đăng ký vào epoll. */
int  pool_start(const Config *cfg, int *event_fd);

/* Đẩy việc sang thread pool. Trả về -1 nếu hàng đợi đầy (server quá tải). */
int  pool_submit_read(Job *job);

/* Đẩy việc sang DB writer (giữ đúng thứ tự FIFO). */
void pool_submit_write(Job *job);

/* Lấy toàn bộ Job đã xong (danh sách nối qua ->next). */
Job *pool_take_results(void);

/* 1 nếu ghi DB đã lỗi liên tục và server chuyển sang chế độ chỉ đọc. */
int  pool_readonly(void);

/* 1 nếu các luồng đang chạy (log chỉ được ghi khi đó). */
int  pool_running(void);

/* Dừng: bỏ việc đọc còn treo, chờ DB writer ghi hết hàng đợi. */
void pool_stop(void);

Job *job_new(JobType type);
void job_free(Job *job);

#endif
