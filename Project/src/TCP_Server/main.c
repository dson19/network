#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "auth.h"
#include "cache.h"
#include "config.h"
#include "conn.h"
#include "log.h"
#include "protocol.h"
#include "session.h"
#include "storage.h"

static void usage(const char *prog)
{
    fprintf(stderr,
            "Usage: %s [-c config] [-p port] [--port-file file]\n"
            "       %s [-c config] --create-admin <username>\n", prog, prog);
}

/* Tạo thư mục cha của path nếu chưa có (1 cấp, ví dụ "data/" trong "data/game.db"). */
static int ensure_parent_dir(const char *path)
{
    char dir[256];
    snprintf(dir, sizeof dir, "%s", path);
    char *slash = strrchr(dir, '/');
    if (!slash || slash == dir)
        return 0;
    *slash = '\0';
    if (mkdir(dir, 0755) != 0 && errno != EEXIST) {
        fprintf(stderr, "cannot create directory %s: %s\n", dir, strerror(errno));
        return -1;
    }
    return 0;
}

/* Đọc 1 dòng từ stdin; nếu là terminal thì tắt hiện ký tự. */
static int read_secret(const char *prompt, char *buf, size_t cap)
{
    struct termios old, quiet;
    int tty = isatty(STDIN_FILENO);
    fputs(prompt, stderr);
    fflush(stderr);
    if (tty && tcgetattr(STDIN_FILENO, &old) == 0) {
        quiet = old;
        quiet.c_lflag &= (tcflag_t)~ECHO;
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &quiet);
    }
    char *r = fgets(buf, (int)cap, stdin);
    if (tty) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &old);
        fputc('\n', stderr);
    }
    if (!r)
        return -1;
    buf[strcspn(buf, "\r\n")] = '\0';
    return 0;
}

static int create_admin(const Config *cfg, const char *name)
{
    if (!proto_valid_username(name)) {
        fprintf(stderr, "Invalid username (3-20 characters: a-z A-Z 0-9 _)\n");
        return 1;
    }
    sqlite3 *db;
    if (storage_open(cfg->db_path, &db) != 0)
        return 1;

    int rc = 1;
    int64_t id;
    int st = storage_user_find(db, name, &id);
    if (st == STORAGE_OK) {
        char ans[16] = "";
        fprintf(stderr, "Account '%s' already exists. Promote it to ADMIN? [y/N] ", name);
        if (fgets(ans, sizeof ans, stdin) && (ans[0] == 'y' || ans[0] == 'Y')) {
            if (storage_user_set_role(db, id, "ADMIN") == STORAGE_OK) {
                printf("Account '%s' is now ADMIN.\n", name);
                rc = 0;
            } else {
                fprintf(stderr, "Database error.\n");
            }
        } else {
            printf("Nothing changed.\n");
            rc = 0;
        }
    } else if (st == STORAGE_NOT_FOUND) {
        char pw[PASSWORD_MAX + 2], pw2[PASSWORD_MAX + 2];
        uint8_t salt[SALT_LEN], hash[HASH_LEN];
        if (read_secret("Password: ", pw, sizeof pw) != 0 ||
            read_secret("Repeat password: ", pw2, sizeof pw2) != 0) {
            fprintf(stderr, "No password given.\n");
        } else if (strcmp(pw, pw2) != 0) {
            fprintf(stderr, "Passwords do not match.\n");
        } else if (!proto_valid_password(pw)) {
            fprintf(stderr, "Password must be %d-%d characters.\n", PASSWORD_MIN, PASSWORD_MAX);
        } else if (auth_new_salt(salt) != 0 ||
                   auth_hash(pw, salt, cfg->pbkdf2_iterations, hash) != 0 ||
                   storage_user_create(db, name, hash, salt, cfg->pbkdf2_iterations, "ADMIN", &id) != STORAGE_OK) {
            fprintf(stderr, "Cannot create account.\n");
        } else {
            printf("Admin account '%s' created.\n", name);
            rc = 0;
        }
        auth_wipe(pw, sizeof pw);
        auth_wipe(pw2, sizeof pw2);
    } else {
        fprintf(stderr, "Database error.\n");
    }
    storage_close(db);
    return rc;
}

int main(int argc, char **argv)
{
    const char *config_path = NULL, *port_file = NULL, *admin_name = NULL;
    int port_override = -1;

    for (int i = 1; i < argc; i++) {
        const char *a = argv[i];
        const char *v = i + 1 < argc ? argv[i + 1] : NULL;
        if (strcmp(a, "-c") == 0 && v) {
            config_path = v;
            i++;
        } else if (strcmp(a, "-p") == 0 && v) {
            char *end;
            long p = strtol(v, &end, 10);
            if (*end != '\0' || p < 0 || p > 65535) {
                fprintf(stderr, "Invalid port: %s\n", v);
                return 1;
            }
            port_override = (int)p;
            i++;
        } else if (strcmp(a, "--port-file") == 0 && v) {
            port_file = v;
            i++;
        } else if (strcmp(a, "--create-admin") == 0 && v) {
            admin_name = v;
            i++;
        } else {
            usage(argv[0]);
            return 1;
        }
    }

    Config cfg;
    config_defaults(&cfg);
    if (config_load(&cfg, config_path ? config_path : "server.conf", config_path != NULL) != 0)
        return 1;
    if (port_override >= 0)
        cfg.port = port_override;

    log_set_timezone();
    if (ensure_parent_dir(cfg.db_path) != 0 || ensure_parent_dir(cfg.log_path) != 0)
        return 1;

    if (admin_name)
        return create_admin(&cfg, admin_name);

    cache_init();
    if (session_load_users(cfg.db_path) != 0)
        return 1;

    /* Chặn SIGINT/SIGTERM ở mọi luồng; luồng chính nhận qua signalfd. */
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGINT);
    sigaddset(&set, SIGTERM);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
    signal(SIGPIPE, SIG_IGN);

    int rc = loop_run(&cfg, port_file);
    cache_free();
    printf("Server stopped.\n");
    return rc == 0 ? 0 : 1;
}
