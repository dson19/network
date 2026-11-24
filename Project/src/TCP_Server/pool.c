#include "pool.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <time.h>
#include <unistd.h>

#include "storage.h"

typedef struct {
    Job *head, *tail;
    int  len;
} Queue;

static void q_push(Queue *q, Job *j)
{
    j->next = NULL;
    if (q->tail)
        q->tail->next = j;
    else
        q->head = j;
    q->tail = j;
    q->len++;
}

static Job *q_pop(Queue *q)
{
    Job *j = q->head;
    if (j) {
        q->head = j->next;
        if (!q->head)
            q->tail = NULL;
        q->len--;
        j->next = NULL;
    }
    return j;
}

static pthread_mutex_t read_mu  = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  read_cv  = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t write_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  write_cv = PTHREAD_COND_INITIALIZER;
static pthread_mutex_t res_mu   = PTHREAD_MUTEX_INITIALIZER;

static Queue read_q, write_q, res_q;
static int read_q_max;
static int read_stopping;          /* đọc/ghi dưới read_mu */
static int write_stopping;         /* đọc/ghi dưới write_mu */

static pthread_t *workers;
static int n_workers;
static pthread_t writer;
static int efd = -1;
static int log_fd = -1;
static char db_path[256];
static atomic_int readonly_flag;
static atomic_int running_flag;

/* ------------------------------------------------------------------ */

Job *job_new(JobType type)
{
    Job *j = calloc(1, sizeof *j);
    if (j)
        j->type = type;
    return j;
}

void job_free(Job *j)
{
    if (!j)
        return;
    auth_wipe(j->pw, sizeof j->pw);
    auth_wipe(j->pw2, sizeof j->pw2);
    free(j->text);
    free(j);
}

static void push_result(Job *j)
{
    auth_wipe(j->pw, sizeof j->pw);       /* luồng chính không cần mật khẩu nữa */
    auth_wipe(j->pw2, sizeof j->pw2);
    pthread_mutex_lock(&res_mu);
    q_push(&res_q, j);
    pthread_mutex_unlock(&res_mu);
    uint64_t one = 1;
    ssize_t r = write(efd, &one, sizeof one);
    (void)r;
}

static void sleep_ms(long ms)
{
    struct timespec ts = { ms / 1000, (ms % 1000) * 1000000L };
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR)
        ;
}

/* ---------------------------- thread pool ---------------------------- */

static void do_verify(sqlite3 *db, Job *j)
{
    uint8_t salt[SALT_LEN] = {0}, stored[HASH_LEN] = {0}, calc[HASH_LEN];
    int iters = j->iterations;
    int found = 0;

    if (j->user_id > 0) {
        int st = storage_user_get_credentials(db, j->user_id, salt, stored, &iters);
        if (st == STORAGE_ERROR) {
            j->status = JOB_STATUS_ERROR;
            return;
        }
        found = st == STORAGE_OK;
    }
    /* Vẫn băm khi tài khoản không tồn tại để thời gian trả lời như nhau. */
    if (auth_hash(j->pw, salt, iters, calc) != 0) {
        j->status = JOB_STATUS_ERROR;
        return;
    }
    j->ok = found && auth_equal(calc, stored);
    j->status = JOB_STATUS_OK;
}

static void do_hash_new(Job *j, const char *pw)
{
    if (auth_new_salt(j->salt) != 0 || auth_hash(pw, j->salt, j->iterations, j->hash) != 0)
        j->status = JOB_STATUS_ERROR;
    else
        j->status = JOB_STATUS_OK;
}

static void *worker_main(void *arg)
{
    (void)arg;
    sqlite3 *db = NULL;
    if (storage_open(db_path, &db) != 0)
        db = NULL;

    for (;;) {
        pthread_mutex_lock(&read_mu);
        while (!read_q.head && !read_stopping)
            pthread_cond_wait(&read_cv, &read_mu);
        if (read_stopping) {
            pthread_mutex_unlock(&read_mu);
            break;
        }
        Job *j = q_pop(&read_q);
        pthread_mutex_unlock(&read_mu);

        if (db == NULL) {
            j->status = JOB_STATUS_ERROR;
        } else {
            switch (j->type) {
            case JOB_VERIFY_LOGIN:
                do_verify(db, j);
                break;
            case JOB_HASH_NEW:
                do_hash_new(j, j->pw);
                break;
            case JOB_CHANGE_PW:
                do_verify(db, j);
                if (j->status == JOB_STATUS_OK && j->ok)
                    do_hash_new(j, j->pw2);
                break;
            default:
                j->status = JOB_STATUS_ERROR;
                break;
            }
        }
        push_result(j);
    }
    storage_close(db);
    return NULL;
}

/* ------------------------------ DB writer ------------------------------ */

static void write_log_text(const char *text)
{
    if (log_fd < 0 || !text)
        return;
    size_t n = strlen(text), off = 0;
    while (off < n) {
        ssize_t w = write(log_fd, text + off, n - off);
        if (w < 0) {
            if (errno == EINTR)
                continue;
            return;
        }
        off += (size_t)w;
    }
}

static int run_write(sqlite3 *db, Job *j)
{
    switch (j->type) {
    case JOB_W_INSERT_USER:
        return storage_user_create(db, j->username, j->hash, j->salt, j->iterations, j->role,
                                   &j->out_id);
    case JOB_W_SET_PASSWORD:
        return storage_user_set_password(db, j->user_id, j->hash, j->salt, j->iterations);
    case JOB_W_TOUCH_LOGIN:
        return storage_user_touch_login(db, j->user_id, j->when);
    default:
        return STORAGE_ERROR;
    }
}

static void enter_readonly(void)
{
    if (atomic_exchange(&readonly_flag, 1) == 0) {
        char line[160];
        time_t now = time(NULL);
        struct tm tm;
        localtime_r(&now, &tm);
        char ts[32];
        strftime(ts, sizeof ts, "%d/%m/%Y %H:%M:%S", &tm);
        snprintf(line, sizeof line, "[%s]$SERVER$DB_READONLY database write failed 3 times\n", ts);
        write_log_text(line);
    }
}

static void *writer_main(void *arg)
{
    (void)arg;
    sqlite3 *db = NULL;
    if (storage_open(db_path, &db) != 0) {
        db = NULL;
        enter_readonly();
    }

    for (;;) {
        pthread_mutex_lock(&write_mu);
        while (!write_q.head && !write_stopping)
            pthread_cond_wait(&write_cv, &write_mu);
        Job *j = q_pop(&write_q);
        pthread_mutex_unlock(&write_mu);
        if (!j)
            break;                         /* stopping và đã ghi hết */

        if (j->type == JOB_W_LOG) {
            write_log_text(j->text);
            job_free(j);
            continue;
        }

        int res = STORAGE_ERROR;
        if (db != NULL && !atomic_load(&readonly_flag)) {
            for (int attempt = 0; attempt < 3; attempt++) {
                res = run_write(db, j);
                if (res != STORAGE_ERROR)
                    break;
                sleep_ms(100);
            }
            if (res == STORAGE_ERROR)
                enter_readonly();
        }
        j->status = res == STORAGE_OK ? JOB_STATUS_OK
                  : res == STORAGE_CONSTRAINT ? JOB_STATUS_CONSTRAINT
                  : JOB_STATUS_ERROR;

        if (j->type == JOB_W_TOUCH_LOGIN)
            job_free(j);
        else
            push_result(j);
    }
    storage_close(db);
    return NULL;
}

/* ------------------------------- API ------------------------------- */

int pool_start(const Config *cfg, int *event_fd)
{
    snprintf(db_path, sizeof db_path, "%s", cfg->db_path);
    read_q_max = cfg->job_queue_max;
    read_stopping = write_stopping = 0;

    efd = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    if (efd < 0) {
        perror("eventfd");
        return -1;
    }
    log_fd = open(cfg->log_path, O_WRONLY | O_CREAT | O_APPEND | O_CLOEXEC, 0644);
    if (log_fd < 0) {
        fprintf(stderr, "cannot open log file %s: %s\n", cfg->log_path, strerror(errno));
        return -1;
    }

    n_workers = cfg->read_workers;
    workers = calloc((size_t)n_workers, sizeof *workers);
    if (!workers)
        return -1;
    for (int i = 0; i < n_workers; i++)
        if (pthread_create(&workers[i], NULL, worker_main, NULL) != 0)
            return -1;
    if (pthread_create(&writer, NULL, writer_main, NULL) != 0)
        return -1;

    atomic_store(&running_flag, 1);
    *event_fd = efd;
    return 0;
}

int pool_submit_read(Job *j)
{
    pthread_mutex_lock(&read_mu);
    if (read_q.len >= read_q_max) {
        pthread_mutex_unlock(&read_mu);
        return -1;
    }
    q_push(&read_q, j);
    pthread_cond_signal(&read_cv);
    pthread_mutex_unlock(&read_mu);
    return 0;
}

void pool_submit_write(Job *j)
{
    pthread_mutex_lock(&write_mu);
    q_push(&write_q, j);
    pthread_cond_signal(&write_cv);
    pthread_mutex_unlock(&write_mu);
}

Job *pool_take_results(void)
{
    pthread_mutex_lock(&res_mu);
    Job *all = res_q.head;
    res_q.head = res_q.tail = NULL;
    res_q.len = 0;
    pthread_mutex_unlock(&res_mu);
    return all;
}

int pool_readonly(void)
{
    return atomic_load(&readonly_flag);
}

int pool_running(void)
{
    return atomic_load(&running_flag);
}

void pool_stop(void)
{
    if (!atomic_load(&running_flag))
        return;

    pthread_mutex_lock(&read_mu);
    read_stopping = 1;
    pthread_cond_broadcast(&read_cv);
    pthread_mutex_unlock(&read_mu);
    for (int i = 0; i < n_workers; i++)
        pthread_join(workers[i], NULL);

    pthread_mutex_lock(&write_mu);
    write_stopping = 1;
    pthread_cond_broadcast(&write_cv);
    pthread_mutex_unlock(&write_mu);
    pthread_join(writer, NULL);            /* writer chỉ thoát khi hàng đợi đã trống */
    atomic_store(&running_flag, 0);

    Job *j;
    while ((j = q_pop(&read_q)) != NULL)
        job_free(j);
    Job *r = pool_take_results();
    while (r) {
        Job *next = r->next;
        job_free(r);
        r = next;
    }
    free(workers);
    workers = NULL;
    if (log_fd >= 0)
        close(log_fd);
    if (efd >= 0)
        close(efd);
    log_fd = efd = -1;
}
