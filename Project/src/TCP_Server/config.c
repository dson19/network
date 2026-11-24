#include "config.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void config_defaults(Config *cfg)
{
    memset(cfg, 0, sizeof *cfg);
    strcpy(cfg->bind_address, "0.0.0.0");
    cfg->port = 5500;
    cfg->send_buffer_max = 262144;
    cfg->read_workers = 2;
    cfg->job_queue_max = 10000;
    strcpy(cfg->db_path, "data/game.db");
    strcpy(cfg->log_path, "logs/server.log");
    cfg->pbkdf2_iterations = 100000;
}

static char *trim(char *s)
{
    while (isspace((unsigned char)*s))
        s++;
    char *end = s + strlen(s);
    while (end > s && isspace((unsigned char)end[-1]))
        *--end = '\0';
    return s;
}

static int parse_int(const char *v, long min, long max, long *out)
{
    char *end;
    errno = 0;
    long x = strtol(v, &end, 10);
    if (errno != 0 || end == v || *end != '\0' || x < min || x > max)
        return -1;
    *out = x;
    return 0;
}

static int set_str(char *dst, size_t cap, const char *v)
{
    if (strlen(v) == 0 || strlen(v) >= cap)
        return -1;
    strcpy(dst, v);
    return 0;
}

int config_load(Config *cfg, const char *path, int must_exist)
{
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        if (!must_exist)
            return 0;
        fprintf(stderr, "config: cannot open %s\n", path);
        return -1;
    }

    char raw[512];
    int lineno = 0, rc = 0;
    while (fgets(raw, sizeof raw, f) != NULL) {
        lineno++;
        char *hash = strchr(raw, '#');
        if (hash != NULL)
            *hash = '\0';
        char *line = trim(raw);
        if (*line == '\0')
            continue;

        char *eq = strchr(line, '=');
        if (eq == NULL) {
            fprintf(stderr, "config: %s:%d: missing '='\n", path, lineno);
            rc = -1;
            break;
        }
        *eq = '\0';
        char *key = trim(line);
        char *val = trim(eq + 1);
        long n = 0;
        int bad = 0;

        if (strcmp(key, "bind_address") == 0)
            bad = set_str(cfg->bind_address, sizeof cfg->bind_address, val);
        else if (strcmp(key, "port") == 0)
            bad = parse_int(val, 0, 65535, &n) || ((cfg->port = (int)n), 0);
        else if (strcmp(key, "send_buffer_max") == 0)
            bad = parse_int(val, 16384, 64L * 1024 * 1024, &n) || ((cfg->send_buffer_max = (size_t)n), 0);
        else if (strcmp(key, "read_workers") == 0)
            bad = parse_int(val, 1, 64, &n) || ((cfg->read_workers = (int)n), 0);
        else if (strcmp(key, "job_queue_max") == 0)
            bad = parse_int(val, 1, 1000000, &n) || ((cfg->job_queue_max = (int)n), 0);
        else if (strcmp(key, "db_path") == 0)
            bad = set_str(cfg->db_path, sizeof cfg->db_path, val);
        else if (strcmp(key, "log_path") == 0)
            bad = set_str(cfg->log_path, sizeof cfg->log_path, val);
        else if (strcmp(key, "pbkdf2_iterations") == 0)
            bad = parse_int(val, 1000, 10000000, &n) || ((cfg->pbkdf2_iterations = (int)n), 0);
        else
            fprintf(stderr, "config: %s:%d: unknown key '%s' (ignored)\n", path, lineno, key);

        if (bad) {
            fprintf(stderr, "config: %s:%d: invalid value for '%s'\n", path, lineno, key);
            rc = -1;
            break;
        }
    }
    fclose(f);
    return rc;
}
