/*
 * Vòng lặp chính: 1 luồng dùng epoll theo dõi socket lắng nghe, mọi socket
 * client, eventfd của pool và signalfd (SIGINT/SIGTERM).
 * Đây là luồng duy nhất đọc/sửa state (kết nối, phiên, tài khoản trong RAM).
 * epoll dùng chế độ level-triggered (mặc định).
 */
#include "conn.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/socket.h>
#include <unistd.h>

#include "cache.h"
#include "dispatch.h"
#include "log.h"
#include "pool.h"
#include "session.h"

static const Config *g_cfg;
static int g_epfd = -1;
static Conn **g_conns;          /* chỉ mục theo fd */
static size_t g_conns_cap;
static uint64_t g_next_id = 1;
static Conn *g_dead;            /* đã đóng, giải phóng cuối mỗi vòng */
static int g_reserve_fd = -1;   /* fd dự phòng để xử lý EMFILE */

/* Đánh dấu các fd đặc biệt trong epoll_event.data.ptr */
static char TAG_LISTEN, TAG_EVENT, TAG_SIGNAL;

const Config *server_config(void)
{
    return g_cfg;
}

/* ----------------------------- tiện ích ----------------------------- */

static void update_events(Conn *c)
{
    if (c->state == CS_CLOSING)
        return;
    struct epoll_event ev = {0};
    ev.events = (c->busy ? 0 : EPOLLIN) | (c->want_write ? EPOLLOUT : 0);
    ev.data.ptr = c;
    epoll_ctl(g_epfd, EPOLL_CTL_MOD, c->fd, &ev);
}

Conn *conn_find(int fd, uint64_t id)
{
    if (fd < 0 || (size_t)fd >= g_conns_cap)
        return NULL;
    Conn *c = g_conns[fd];
    return (c && c->id == id && c->state != CS_CLOSING) ? c : NULL;
}

static int ensure_slot(int fd)
{
    if ((size_t)fd < g_conns_cap)
        return 0;
    size_t cap = g_conns_cap ? g_conns_cap : 1024;
    while (cap <= (size_t)fd)
        cap *= 2;
    Conn **nc = realloc(g_conns, cap * sizeof *nc);
    if (!nc)
        return -1;
    memset(nc + g_conns_cap, 0, (cap - g_conns_cap) * sizeof *nc);
    g_conns = nc;
    g_conns_cap = cap;
    return 0;
}

/* ------------------------------ gửi ------------------------------ */

static void flush_out(Conn *c)
{
    while (c->out_off < c->out_len) {
        ssize_t n = send(c->fd, c->out + c->out_off, c->out_len - c->out_off, MSG_NOSIGNAL);
        if (n > 0) {
            c->out_off += (size_t)n;
            continue;
        }
        if (n < 0 && errno == EINTR)
            continue;
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
            break;
        conn_close(c, errno == ECONNRESET || errno == EPIPE ? "reset" : "send_error");
        return;
    }
    int pending = c->out_off < c->out_len;
    if (!pending)
        c->out_off = c->out_len = 0;
    if (pending != c->want_write) {
        c->want_write = pending;
        update_events(c);
    }
}

void conn_send_line(Conn *c, const char *line)
{
    if (c->state == CS_CLOSING)
        return;
    size_t n = strlen(line);
    size_t pending = c->out_len - c->out_off;
    if (pending + n + 2 > g_cfg->send_buffer_max) {
        conn_close(c, "send_buffer_full");
        return;
    }
    if (c->out_off > 0) {                  /* dồn phần chưa gửi về đầu buffer */
        memmove(c->out, c->out + c->out_off, pending);
        c->out_off = 0;
        c->out_len = pending;
    }
    if (c->out_len + n + 2 > c->out_cap) {
        size_t cap = c->out_cap ? c->out_cap : 4096;
        while (cap < c->out_len + n + 2)
            cap *= 2;
        char *nb = realloc(c->out, cap);
        if (!nb) {
            conn_close(c, "out_of_memory");
            return;
        }
        c->out = nb;
        c->out_cap = cap;
    }
    memcpy(c->out + c->out_len, line, n);
    memcpy(c->out + c->out_len + n, "\r\n", 2);
    c->out_len += n + 2;
    flush_out(c);
}

/* --------------------------- đóng kết nối --------------------------- */

void conn_close(Conn *c, const char *reason)
{
    if (c->state == CS_CLOSING)
        return;
    session_on_close(c);                   /* hủy phiên nếu có */
    c->state = CS_CLOSING;
    epoll_ctl(g_epfd, EPOLL_CTL_DEL, c->fd, NULL);

    /* Đóng "êm": gửi FIN sau phần đã gửi, rồi đọc bỏ dữ liệu client gửi dở.
     * Nếu close() khi còn dữ liệu chưa đọc, kernel gửi RST và client có thể
     * mất luôn message cuối (ví dụ response 2004, FORCE_LOGOUT). */
    shutdown(c->fd, SHUT_WR);
    char junk[4096];
    for (int i = 0; i < 16 && recv(c->fd, junk, sizeof junk, MSG_DONTWAIT) > 0; i++)
        ;
    close(c->fd);
    g_conns[c->fd] = NULL;

    char what[64];
    snprintf(what, sizeof what, "DISCONNECT %s", reason);
    log_conn(c, what);

    c->next_dead = g_dead;
    g_dead = c;
}

static void free_dead(void)
{
    while (g_dead) {
        Conn *c = g_dead;
        g_dead = c->next_dead;
        free(c->out);
        free(c->req_text);
        free(c);
    }
}

/* ------------------------------ nhận ------------------------------ */

static void message_too_long(Conn *c)
{
    free(c->req_text);
    c->req_text = strdup("(message too long)");
    c->req_log = 1;
    respond(c, RC_MSG_TOO_LONG, NULL, NULL);
    conn_close(c, "too_long");
}

/* Xử lý các message hoàn chỉnh đang có trong buffer nhận. */
static void process_lines(Conn *c)
{
    char line[LINEBUF_CAP + 1];            /* không dùng static: hàm có thể được gọi lồng */
    while (c->state != CS_CLOSING && !c->busy) {
        size_t len;
        int r = linebuf_next(&c->in, line, sizeof line, &len);
        if (r == 0)
            return;
        if (r < 0) {
            message_too_long(c);
            return;
        }
        if (len > 0)                       /* bỏ qua dòng trống */
            dispatch_line(c, line, len);
    }
}

void conn_set_busy(Conn *c, int busy)
{
    if (c->state == CS_CLOSING)
        return;
    c->busy = busy;
    update_events(c);                      /* đang bận thì ngừng đọc socket */
    if (!busy)
        process_lines(c);
}

static void on_readable(Conn *c)
{
    while (c->state != CS_CLOSING && !c->busy) {
        size_t avail;
        char *p = linebuf_space(&c->in, &avail);
        if (avail == 0) {                  /* đầy mà process_lines không lấy ra được gì */
            message_too_long(c);
            return;
        }
        ssize_t n = recv(c->fd, p, avail, 0);
        if (n > 0) {
            linebuf_commit(&c->in, (size_t)n);
            process_lines(c);
            continue;
        }
        if (n == 0) {
            conn_close(c, "closed");
            return;
        }
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return;
        conn_close(c, errno == ECONNRESET ? "reset" : "recv_error");
        return;
    }
}

/* ------------------------------ accept ------------------------------ */

static void on_accept(int lfd)
{
    for (;;) {
        struct sockaddr_in addr;
        socklen_t alen = sizeof addr;
        int fd = accept4(lfd, (struct sockaddr *)&addr, &alen, SOCK_NONBLOCK | SOCK_CLOEXEC);
        if (fd < 0) {
            if (errno == EINTR)
                continue;
            if (errno == EMFILE || errno == ENFILE) {
                /* Hết fd: nhả fd dự phòng để accept rồi đóng ngay, tránh epoll báo mãi. */
                if (g_reserve_fd >= 0) {
                    close(g_reserve_fd);
                    int tmp = accept(lfd, NULL, NULL);
                    if (tmp >= 0)
                        close(tmp);
                    g_reserve_fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
                }
                log_server("ACCEPT_FAILED too many open files");
            }
            return;                        /* EAGAIN: đã nhận hết */
        }

        Conn *c = calloc(1, sizeof *c);
        if (!c || ensure_slot(fd) != 0) {
            free(c);
            close(fd);
            continue;
        }
        c->fd = fd;
        c->id = g_next_id++;
        c->state = CS_CONNECTED;
        linebuf_init(&c->in);
        char ip[INET_ADDRSTRLEN] = "?";
        inet_ntop(AF_INET, &addr.sin_addr, ip, sizeof ip);
        snprintf(c->peer, sizeof c->peer, "%s:%u", ip, (unsigned)ntohs(addr.sin_port));

        struct epoll_event ev = {0};
        ev.events = EPOLLIN;
        ev.data.ptr = c;
        if (epoll_ctl(g_epfd, EPOLL_CTL_ADD, fd, &ev) != 0) {
            close(fd);
            free(c);
            continue;
        }
        g_conns[fd] = c;
        log_conn(c, "CONNECT");
    }
}

/* ------------------------------ khởi động ------------------------------ */

static int open_listener(const Config *cfg, const char *port_file)
{
    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)cfg->port);
    if (inet_pton(AF_INET, cfg->bind_address, &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid bind_address: %s\n", cfg->bind_address);
        close(fd);
        return -1;
    }
    if (bind(fd, (struct sockaddr *)&addr, sizeof addr) != 0 || listen(fd, SOMAXCONN) != 0) {
        perror("bind/listen");
        close(fd);
        return -1;
    }

    socklen_t alen = sizeof addr;
    getsockname(fd, (struct sockaddr *)&addr, &alen);
    int port = ntohs(addr.sin_port);
    printf("Server listening on %s:%d\n", cfg->bind_address, port);
    fflush(stdout);
    log_server("START port=%d", port);

    if (port_file) {
        FILE *f = fopen(port_file, "w");
        if (f) {
            fprintf(f, "%d\n", port);
            fclose(f);
        }
    }
    return fd;
}

static int add_special(int fd, char *tag)
{
    struct epoll_event ev = {0};
    ev.events = EPOLLIN;
    ev.data.ptr = tag;
    return epoll_ctl(g_epfd, EPOLL_CTL_ADD, fd, &ev);
}

static void on_pool_results(int efd)
{
    uint64_t cnt;
    while (read(efd, &cnt, sizeof cnt) > 0)
        ;
    Job *j = pool_take_results();
    while (j) {
        Job *next = j->next;
        j->next = NULL;
        session_on_result(j);              /* giải phóng hoặc gửi tiếp job */
        j = next;
    }
}

static void shutdown_all(int sig)
{
    log_server("SHUTDOWN signal=%d", sig);
    for (size_t fd = 0; fd < g_conns_cap; fd++) {
        Conn *c = g_conns[fd];
        if (c && c->state != CS_CLOSING) {
            session_force_logout(c, "SHUTDOWN");
            conn_close(c, "shutdown");
        }
    }
    free_dead();
}

int loop_run(const Config *cfg, const char *port_file)
{
    g_cfg = cfg;

    /* SIGINT/SIGTERM đã bị chặn ở main (trước khi tạo luồng); nhận qua signalfd. */
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    int sfd = signalfd(-1, &set, SFD_NONBLOCK | SFD_CLOEXEC);

    int efd;
    if (sfd < 0 || pool_start(cfg, &efd) != 0)
        return -1;

    int lfd = open_listener(cfg, port_file);
    g_epfd = epoll_create1(EPOLL_CLOEXEC);
    if (lfd < 0 || g_epfd < 0)
        return -1;
    g_reserve_fd = open("/dev/null", O_RDONLY | O_CLOEXEC);
    add_special(lfd, &TAG_LISTEN);
    add_special(efd, &TAG_EVENT);
    add_special(sfd, &TAG_SIGNAL);

    struct epoll_event events[256];
    int sig = 0;
    while (!sig) {
        int n = epoll_wait(g_epfd, events, 256, -1);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            perror("epoll_wait");
            break;
        }
        for (int i = 0; i < n; i++) {
            void *p = events[i].data.ptr;
            uint32_t e = events[i].events;
            if (p == &TAG_LISTEN) {
                on_accept(lfd);
            } else if (p == &TAG_EVENT) {
                on_pool_results(efd);
            } else if (p == &TAG_SIGNAL) {
                struct signalfd_siginfo si;
                if (read(sfd, &si, sizeof si) == sizeof si)
                    sig = (int)si.ssi_signo;
            } else {
                Conn *c = p;
                if (c->state == CS_CLOSING)
                    continue;
                if (e & EPOLLIN)
                    on_readable(c);        /* đọc trước để không bỏ sót dữ liệu cuối */
                if (c->state != CS_CLOSING && (e & (EPOLLERR | EPOLLHUP)) && !(e & EPOLLIN))
                    conn_close(c, "hangup");
                if (c->state != CS_CLOSING && (e & EPOLLOUT))
                    flush_out(c);
            }
        }
        free_dead();
    }

    close(lfd);
    shutdown_all(sig);
    pool_stop();                           /* chờ DB writer ghi hết */
    close(g_epfd);
    close(sfd);
    free(g_conns);
    return 0;
}
