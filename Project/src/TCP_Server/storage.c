#include "storage.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

#define SCHEMA_VERSION 1

static const char *SCHEMA_V1 =
    "CREATE TABLE users ("
    "  id            INTEGER PRIMARY KEY,"
    "  username      TEXT    NOT NULL UNIQUE COLLATE NOCASE,"
    "  pass_hash     BLOB    NOT NULL,"
    "  salt          BLOB    NOT NULL,"
    "  iterations    INTEGER NOT NULL,"
    "  role          TEXT    NOT NULL DEFAULT 'PLAYER' CHECK (role IN ('PLAYER','ADMIN')),"
    "  locked        INTEGER NOT NULL DEFAULT 0 CHECK (locked IN (0,1)),"
    "  created_at    INTEGER NOT NULL,"
    "  last_login_at INTEGER"
    ");"
    "CREATE TABLE questions ("
    "  id       INTEGER PRIMARY KEY,"
    "  tier     TEXT    NOT NULL CHECK (tier IN ('BRONZE','SILVER','GOLD')),"
    "  question TEXT    NOT NULL,"
    "  choice_a TEXT    NOT NULL,"
    "  choice_b TEXT    NOT NULL,"
    "  choice_c TEXT    NOT NULL,"
    "  choice_d TEXT    NOT NULL,"
    "  answer   TEXT    NOT NULL CHECK (answer IN ('A','B','C','D')),"
    "  active   INTEGER NOT NULL DEFAULT 1 CHECK (active IN (0,1))"
    ");"
    "CREATE INDEX idx_questions_tier ON questions(tier, active);";

static int map_rc(int rc)
{
    if (rc == SQLITE_DONE || rc == SQLITE_OK)
        return STORAGE_OK;
    if ((rc & 0xff) == SQLITE_CONSTRAINT)
        return STORAGE_CONSTRAINT;
    return STORAGE_ERROR;
}

static int exec(sqlite3 *db, const char *sql)
{
    char *err = NULL;
    int rc = sqlite3_exec(db, sql, NULL, NULL, &err);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "storage: %s\n", err ? err : sqlite3_errmsg(db));
        sqlite3_free(err);
        return -1;
    }
    return 0;
}

static int user_version(sqlite3 *db)
{
    sqlite3_stmt *st;
    int v = -1;
    if (sqlite3_prepare_v2(db, "PRAGMA user_version", -1, &st, NULL) != SQLITE_OK)
        return -1;
    if (sqlite3_step(st) == SQLITE_ROW)
        v = sqlite3_column_int(st, 0);
    sqlite3_finalize(st);
    return v;
}

int storage_open(const char *path, sqlite3 **out)
{
    sqlite3 *db = NULL;
    if (sqlite3_open_v2(path, &db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX,
                        NULL) != SQLITE_OK) {
        fprintf(stderr, "storage: cannot open %s: %s\n", path, db ? sqlite3_errmsg(db) : "?");
        sqlite3_close(db);
        return -1;
    }
    sqlite3_busy_timeout(db, 5000);
    if (exec(db, "PRAGMA foreign_keys = ON;"
                 "PRAGMA journal_mode = WAL;"
                 "PRAGMA synchronous = NORMAL;") != 0)
        goto fail;

    int v = user_version(db);
    if (v < 0)
        goto fail;
    if (v == 0) {
        if (exec(db, "BEGIN IMMEDIATE") != 0)
            goto fail;
        if (user_version(db) == 0) {       /* luồng khác có thể đã tạo trước */
            char sql[64];
            snprintf(sql, sizeof sql, "PRAGMA user_version = %d", SCHEMA_VERSION);
            if (exec(db, SCHEMA_V1) != 0 || exec(db, sql) != 0) {
                exec(db, "ROLLBACK");
                goto fail;
            }
        }
        if (exec(db, "COMMIT") != 0)
            goto fail;
    } else if (v > SCHEMA_VERSION) {
        fprintf(stderr, "storage: database schema v%d is newer than this server (v%d)\n",
                v, SCHEMA_VERSION);
        goto fail;
    }
    *out = db;
    return 0;

fail:
    sqlite3_close(db);
    return -1;
}

void storage_close(sqlite3 *db)
{
    sqlite3_close(db);
}

int storage_load_users(sqlite3 *db, storage_user_cb cb, void *arg)
{
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, "SELECT id, username, role, locked FROM users ORDER BY id",
                           -1, &st, NULL) != SQLITE_OK)
        return STORAGE_ERROR;
    int rc;
    while ((rc = sqlite3_step(st)) == SQLITE_ROW)
        cb(sqlite3_column_int64(st, 0), (const char *)sqlite3_column_text(st, 1),
           (const char *)sqlite3_column_text(st, 2), sqlite3_column_int(st, 3), arg);
    sqlite3_finalize(st);
    return rc == SQLITE_DONE ? STORAGE_OK : STORAGE_ERROR;
}

int storage_user_find(sqlite3 *db, const char *username, int64_t *out_id)
{
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, "SELECT id FROM users WHERE username = ?", -1, &st, NULL) != SQLITE_OK)
        return STORAGE_ERROR;
    sqlite3_bind_text(st, 1, username, -1, SQLITE_TRANSIENT);
    int rc = sqlite3_step(st), res;
    if (rc == SQLITE_ROW) {
        *out_id = sqlite3_column_int64(st, 0);
        res = STORAGE_OK;
    } else {
        res = rc == SQLITE_DONE ? STORAGE_NOT_FOUND : STORAGE_ERROR;
    }
    sqlite3_finalize(st);
    return res;
}

int storage_user_get_credentials(sqlite3 *db, int64_t user_id, uint8_t salt[SALT_LEN],
                                 uint8_t hash[HASH_LEN], int *iterations)
{
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, "SELECT salt, pass_hash, iterations FROM users WHERE id = ?",
                           -1, &st, NULL) != SQLITE_OK)
        return STORAGE_ERROR;
    sqlite3_bind_int64(st, 1, user_id);
    int rc = sqlite3_step(st), res = STORAGE_ERROR;
    if (rc == SQLITE_ROW) {
        if (sqlite3_column_bytes(st, 0) == SALT_LEN && sqlite3_column_bytes(st, 1) == HASH_LEN) {
            memcpy(salt, sqlite3_column_blob(st, 0), SALT_LEN);
            memcpy(hash, sqlite3_column_blob(st, 1), HASH_LEN);
            *iterations = sqlite3_column_int(st, 2);
            res = STORAGE_OK;
        }
    } else if (rc == SQLITE_DONE) {
        res = STORAGE_NOT_FOUND;
    }
    sqlite3_finalize(st);
    return res;
}

int storage_user_create(sqlite3 *db, const char *username, const uint8_t hash[HASH_LEN],
                        const uint8_t salt[SALT_LEN], int iterations, const char *role,
                        int64_t *out_id)
{
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db,
            "INSERT INTO users (username, pass_hash, salt, iterations, role, created_at) "
            "VALUES (?, ?, ?, ?, ?, ?)", -1, &st, NULL) != SQLITE_OK)
        return STORAGE_ERROR;
    sqlite3_bind_text(st, 1, username, -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(st, 2, hash, HASH_LEN, SQLITE_TRANSIENT);
    sqlite3_bind_blob(st, 3, salt, SALT_LEN, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 4, iterations);
    sqlite3_bind_text(st, 5, role, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 6, (int64_t)time(NULL));
    int res = map_rc(sqlite3_step(st));
    if (res == STORAGE_OK)
        *out_id = sqlite3_last_insert_rowid(db);
    sqlite3_finalize(st);
    return res;
}

static int update_one(sqlite3 *db, sqlite3_stmt *st)
{
    int res = map_rc(sqlite3_step(st));
    sqlite3_finalize(st);
    if (res == STORAGE_OK && sqlite3_changes(db) == 0)
        res = STORAGE_NOT_FOUND;
    return res;
}

int storage_user_set_password(sqlite3 *db, int64_t user_id, const uint8_t hash[HASH_LEN],
                              const uint8_t salt[SALT_LEN], int iterations)
{
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, "UPDATE users SET pass_hash = ?, salt = ?, iterations = ? WHERE id = ?",
                           -1, &st, NULL) != SQLITE_OK)
        return STORAGE_ERROR;
    sqlite3_bind_blob(st, 1, hash, HASH_LEN, SQLITE_TRANSIENT);
    sqlite3_bind_blob(st, 2, salt, SALT_LEN, SQLITE_TRANSIENT);
    sqlite3_bind_int(st, 3, iterations);
    sqlite3_bind_int64(st, 4, user_id);
    return update_one(db, st);
}

int storage_user_set_role(sqlite3 *db, int64_t user_id, const char *role)
{
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, "UPDATE users SET role = ? WHERE id = ?", -1, &st, NULL) != SQLITE_OK)
        return STORAGE_ERROR;
    sqlite3_bind_text(st, 1, role, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int64(st, 2, user_id);
    return update_one(db, st);
}

int storage_user_touch_login(sqlite3 *db, int64_t user_id, int64_t when)
{
    sqlite3_stmt *st;
    if (sqlite3_prepare_v2(db, "UPDATE users SET last_login_at = ? WHERE id = ?", -1, &st, NULL) != SQLITE_OK)
        return STORAGE_ERROR;
    sqlite3_bind_int64(st, 1, when);
    sqlite3_bind_int64(st, 2, user_id);
    return update_one(db, st);
}
