/*
 * Tài khoản và phiên: PING, REGISTER, LOGIN, LOGOUT, WHOAMI, CHANGE_PASSWORD.
 *
 * Phiên = cặp con trỏ conn->user và user->conn. Mỗi tài khoản chỉ có 1 phiên:
 * đăng nhập ở kết nối mới thì kết nối cũ nhận FORCE_LOGOUT (REPLACED) và bị đóng.
 *
 * Việc chậm (băm mật khẩu, đọc DB) chạy trên thread pool; việc ghi DB chạy
 * trên DB writer. Trong lúc chờ, kết nối ở trạng thái busy nên các request
 * sau của nó nằm chờ trong buffer nhận, giữ đúng thứ tự.
 */
#include "session.h"

#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>

#include "cache.h"
#include "dispatch.h"
#include "log.h"
#include "protocol.h"
#include "storage.h"

/* ------------------------------ tiện ích ------------------------------ */

static int64_t now_unix_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (int64_t)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

/* Lấy tham số chuỗi; NULL nếu thiếu hoặc sai kiểu. */
static const char *get_str(const cJSON *req, const char *key)
{
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(req, key);
    return cJSON_IsString(v) ? v->valuestring : NULL;
}

static Job *job_for(Conn *c, JobType type)
{
    Job *j = job_new(type);
    if (j) {
        j->fd = c->fd;
        j->conn_id = c->id;
    }
    return j;
}

static void copy_str(char *dst, size_t cap, const char *src)
{
    snprintf(dst, cap, "%s", src);
}

/* Gắn phiên: conn <-> user. */
static void attach(Conn *c, User *u)
{
    c->user = u;
    u->conn = c;
    c->state = CS_LOGGED_IN;
}

/* Gỡ phiên khỏi kết nối. Các bước sau này (rời đội, báo trận...) thêm ở đây. */
static void detach(Conn *c)
{
    if (c->user) {
        if (c->user->conn == c)
            c->user->conn = NULL;
        c->user = NULL;
    }
    if (c->state == CS_LOGGED_IN)
        c->state = CS_CONNECTED;
}

void session_on_close(Conn *c)
{
    detach(c);
}

void session_force_logout(Conn *c, const char *reason)
{
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "reason", reason);
    send_event(c, "FORCE_LOGOUT", data);
    detach(c);
}

/* ------------------------------ nạp từ DB ------------------------------ */

static void load_one(int64_t id, const char *username, const char *role, int locked, void *arg)
{
    int *count = arg;
    if (cache_add(id, username, strcmp(role, "ADMIN") == 0 ? ROLE_ADMIN : ROLE_PLAYER, locked))
        (*count)++;
}

int session_load_users(const char *db_path)
{
    sqlite3 *db;
    if (storage_open(db_path, &db) != 0)
        return -1;
    int count = 0;
    int rc = storage_load_users(db, load_one, &count);
    storage_close(db);
    if (rc != STORAGE_OK)
        return -1;
    printf("Loaded %d account(s) from %s\n", count, db_path);
    return 0;
}

/* -------------------------------- lệnh -------------------------------- */

void cmd_ping(Conn *c, const cJSON *req)
{
    (void)req;
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "time", (double)now_unix_ms());
    respond(c, RC_OK, NULL, data);
}

void cmd_register(Conn *c, const cJSON *req)
{
    if (c->user) {
        respond(c, RC_ALREADY_LOGGED_IN, NULL, NULL);
        return;
    }
    const char *name = get_str(req, "username");
    const char *pw = get_str(req, "password");
    if (!name || !pw) {
        respond(c, RC_BAD_PARAM, NULL, NULL);
        return;
    }
    if (!proto_valid_username(name)) {
        respond(c, RC_INVALID_USERNAME, NULL, NULL);
        return;
    }
    if (!proto_valid_password(pw)) {
        respond(c, RC_INVALID_PASSWORD, NULL, NULL);
        return;
    }
    if (pool_readonly()) {
        respond(c, RC_INTERNAL_ERROR, "Server is read-only", NULL);
        return;
    }
    if (cache_find(name)) {                /* đã có, hoặc đang có người đăng ký tên này */
        respond(c, RC_USERNAME_TAKEN, NULL, NULL);
        return;
    }

    /* Giữ chỗ tên trong RAM để 2 người không đăng ký trùng cùng lúc. */
    User *u = cache_add(0, name, ROLE_PLAYER, 0);
    Job *j = job_for(c, JOB_HASH_NEW);
    if (!u || !j) {
        if (u)
            cache_remove(u);
        job_free(j);
        respond(c, RC_INTERNAL_ERROR, NULL, NULL);
        return;
    }
    u->pending = 1;
    copy_str(j->username, sizeof j->username, name);
    copy_str(j->pw, sizeof j->pw, pw);
    j->iterations = server_config()->pbkdf2_iterations;
    if (pool_submit_read(j) != 0) {
        cache_remove(u);
        job_free(j);
        respond(c, RC_SERVER_BUSY, NULL, NULL);
        return;
    }
    conn_set_busy(c, 1);
}

void cmd_login(Conn *c, const cJSON *req)
{
    if (c->user) {
        respond(c, RC_ALREADY_LOGGED_IN, NULL, NULL);
        return;
    }
    const char *name = get_str(req, "username");
    const char *pw = get_str(req, "password");
    if (!name || !pw) {
        respond(c, RC_BAD_PARAM, NULL, NULL);
        return;
    }

    Job *j = job_for(c, JOB_VERIFY_LOGIN);
    if (!j) {
        respond(c, RC_INTERNAL_ERROR, NULL, NULL);
        return;
    }
    /* Tên sai định dạng hoặc không tồn tại: vẫn cho pool băm thử (user_id = 0)
     * rồi trả 3003, để thời gian trả lời không lộ tên nào có tồn tại. */
    User *u = proto_valid_username(name) ? cache_find(name) : NULL;
    j->user_id = (u && !u->pending) ? u->id : 0;
    copy_str(j->username, sizeof j->username, u ? u->username : "");
    copy_str(j->pw, sizeof j->pw, pw);
    j->iterations = server_config()->pbkdf2_iterations;
    if (pool_submit_read(j) != 0) {
        job_free(j);
        respond(c, RC_SERVER_BUSY, NULL, NULL);
        return;
    }
    c->state = CS_AUTH_PENDING;
    conn_set_busy(c, 1);
}

void cmd_logout(Conn *c, const cJSON *req)
{
    (void)req;
    respond(c, RC_OK, NULL, NULL);         /* ghi log khi vẫn còn biết tên người dùng */
    detach(c);
}

void cmd_whoami(Conn *c, const cJSON *req)
{
    (void)req;
    User *u = c->user;
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "user_id", (double)u->id);
    cJSON_AddStringToObject(data, "username", u->username);
    cJSON_AddStringToObject(data, "role", role_name(u->role));
    cJSON_AddNullToObject(data, "team_id");     /* chưa có chức năng đội */
    cJSON_AddFalseToObject(data, "is_captain");
    cJSON_AddStringToObject(data, "state", "IDLE");
    respond(c, RC_OK, NULL, data);
}

void cmd_change_password(Conn *c, const cJSON *req)
{
    const char *old_pw = get_str(req, "old_password");
    const char *new_pw = get_str(req, "new_password");
    if (!old_pw || !new_pw) {
        respond(c, RC_BAD_PARAM, NULL, NULL);
        return;
    }
    if (!proto_valid_password(new_pw)) {
        respond(c, RC_INVALID_PASSWORD, NULL, NULL);
        return;
    }
    if (pool_readonly()) {
        respond(c, RC_INTERNAL_ERROR, "Server is read-only", NULL);
        return;
    }
    Job *j = job_for(c, JOB_CHANGE_PW);
    if (!j) {
        respond(c, RC_INTERNAL_ERROR, NULL, NULL);
        return;
    }
    j->user_id = c->user->id;
    copy_str(j->pw, sizeof j->pw, old_pw);
    copy_str(j->pw2, sizeof j->pw2, new_pw);
    j->iterations = server_config()->pbkdf2_iterations;
    if (pool_submit_read(j) != 0) {
        job_free(j);
        respond(c, RC_SERVER_BUSY, NULL, NULL);
        return;
    }
    conn_set_busy(c, 1);
}

/* ------------------------- kết quả từ pool/writer ------------------------- */

static void finish_login(Conn *c, Job *j)
{
    c->state = CS_CONNECTED;
    if (j->status != JOB_STATUS_OK) {
        respond(c, RC_INTERNAL_ERROR, NULL, NULL);
        return;
    }
    User *u = j->ok ? cache_find(j->username) : NULL;
    if (!u || u->pending || u->id != j->user_id) {
        respond(c, RC_WRONG_CREDENTIALS, NULL, NULL);
        return;
    }
    if (u->locked) {                       /* chỉ báo khóa khi mật khẩu đúng */
        respond(c, RC_ACCOUNT_LOCKED, NULL, NULL);
        return;
    }

    if (u->conn && u->conn != c) {         /* thay thế phiên cũ */
        Conn *old = u->conn;
        session_force_logout(old, "REPLACED");
        conn_close(old, "replaced");
    }
    attach(c, u);

    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "user_id", (double)u->id);
    cJSON_AddStringToObject(data, "username", u->username);
    cJSON_AddStringToObject(data, "role", role_name(u->role));
    cJSON_AddNullToObject(data, "team_id");     /* chưa có chức năng đội */
    respond(c, RC_OK, NULL, data);

    Job *touch = job_new(JOB_W_TOUCH_LOGIN);
    if (touch) {
        touch->user_id = u->id;
        touch->when = (int64_t)time(NULL);
        pool_submit_write(touch);
    }
}

/* REGISTER bước 2: đã băm xong, chuyển sang DB writer để INSERT. */
static void register_hashed(Conn *c, Job *j)
{
    User *u = cache_find(j->username);
    if (j->status != JOB_STATUS_OK) {
        if (u && u->pending)
            cache_remove(u);
        if (c) {
            respond(c, RC_INTERNAL_ERROR, NULL, NULL);
            conn_set_busy(c, 0);
        }
        job_free(j);
        return;
    }
    /* Kể cả khi client đã ngắt kết nối, vẫn tạo tài khoản cho trọn vẹn. */
    j->type = JOB_W_INSERT_USER;
    copy_str(j->role, sizeof j->role, "PLAYER");
    pool_submit_write(j);
}

/* REGISTER bước 3: DB đã cấp id. */
static void register_inserted(Conn *c, Job *j)
{
    User *u = cache_find(j->username);
    int code = RC_OK;
    const char *msg = NULL;
    if (j->status == JOB_STATUS_OK && u && u->pending) {
        u->id = j->out_id;
        u->pending = 0;
    } else {
        if (u && u->pending)
            cache_remove(u);
        code = j->status == JOB_STATUS_CONSTRAINT ? RC_USERNAME_TAKEN : RC_INTERNAL_ERROR;
        msg = code == RC_INTERNAL_ERROR ? "Server is read-only" : NULL;
    }
    if (c) {
        respond(c, code, msg, NULL);
        conn_set_busy(c, 0);
    }
    job_free(j);
}

/* CHANGE_PASSWORD bước 2: đã kiểm tra mật khẩu cũ và băm mật khẩu mới. */
static void change_checked(Conn *c, Job *j)
{
    if (!c) {                              /* client đã đi: không đổi nữa */
        job_free(j);
        return;
    }
    if (j->status != JOB_STATUS_OK || !j->ok) {
        if (j->status != JOB_STATUS_OK)
            respond(c, RC_INTERNAL_ERROR, NULL, NULL);
        else
            respond(c, RC_WRONG_CREDENTIALS, "Wrong password", NULL);
        conn_set_busy(c, 0);
        job_free(j);
        return;
    }
    /* Trả lời sau khi DB ghi xong, để lần LOGIN kế tiếp chắc chắn đọc mật khẩu mới. */
    j->type = JOB_W_SET_PASSWORD;
    pool_submit_write(j);
}

static void change_saved(Conn *c, Job *j)
{
    if (c) {
        if (j->status == JOB_STATUS_OK)
            respond(c, RC_OK, NULL, NULL);
        else
            respond(c, RC_INTERNAL_ERROR, "Server is read-only", NULL);
        conn_set_busy(c, 0);
    }
    job_free(j);
}

void session_on_result(Job *j)
{
    Conn *c = conn_find(j->fd, j->conn_id);   /* NULL nếu kết nối đã đóng */
    switch (j->type) {
    case JOB_VERIFY_LOGIN:
        if (c) {
            finish_login(c, j);
            conn_set_busy(c, 0);
        }
        job_free(j);
        break;
    case JOB_HASH_NEW:
        register_hashed(c, j);
        break;
    case JOB_W_INSERT_USER:
        register_inserted(c, j);
        break;
    case JOB_CHANGE_PW:
        change_checked(c, j);
        break;
    case JOB_W_SET_PASSWORD:
        change_saved(c, j);
        break;
    default:
        job_free(j);
        break;
    }
}
