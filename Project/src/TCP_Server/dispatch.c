#include "dispatch.h"

#include <stdlib.h>
#include <string.h>

#include "log.h"
#include "protocol.h"
#include "session.h"

typedef void (*Handler)(Conn *c, const cJSON *req);

/* need_login = 1: chưa đăng nhập thì trả 3001 trước khi xét tham số. */
static const struct {
    const char *name;
    int         need_login;
    Handler     fn;
} COMMANDS[] = {
    { "PING",            0, cmd_ping },
    { "REGISTER",        0, cmd_register },
    { "LOGIN",           0, cmd_login },
    { "LOGOUT",          1, cmd_logout },
    { "WHOAMI",          1, cmd_whoami },
    { "CHANGE_PASSWORD", 1, cmd_change_password },
};

/* Bản sao request để ghi log: mọi trường có tên chứa "password" (không phân
 * biệt hoa thường) đều bị thay bằng "***". */
static char *masked_text(const cJSON *req)
{
    cJSON *copy = cJSON_Duplicate(req, 1);
    if (!copy)
        return strdup("?");
    cJSON *it = copy->child;
    while (it) {
        cJSON *next = it->next;
        if (it->string && strcasestr(it->string, "password")) {
            cJSON *mask = cJSON_CreateString("***");
            if (mask && (mask->string = strdup(it->string)) != NULL)
                cJSON_ReplaceItemViaPointer(copy, it, mask);   /* giải phóng `it` */
            else
                cJSON_Delete(mask);
        }
        it = next;
    }
    char *s = cJSON_PrintUnformatted(copy);
    cJSON_Delete(copy);
    return s ? s : strdup("?");
}

void dispatch_line(Conn *c, const char *line, size_t len)
{
    free(c->req_text);
    c->req_text = NULL;
    c->req_log = 1;

    cJSON *req = cJSON_ParseWithLength(line, len);
    if (!req || !cJSON_IsObject(req)) {
        /* Không ghi nguyên dòng vào log vì có thể chứa mật khẩu. */
        c->req_text = strdup("(invalid JSON)");
        respond(c, RC_BAD_FORMAT, NULL, NULL);
        cJSON_Delete(req);
        return;
    }
    c->req_text = masked_text(req);

    const cJSON *cmd = cJSON_GetObjectItemCaseSensitive(req, "cmd");
    const char *name = cJSON_IsString(cmd) ? cmd->valuestring : NULL;
    size_t found = sizeof COMMANDS / sizeof *COMMANDS;
    for (size_t i = 0; name && i < sizeof COMMANDS / sizeof *COMMANDS; i++)
        if (strcmp(COMMANDS[i].name, name) == 0)
            found = i;

    if (found == sizeof COMMANDS / sizeof *COMMANDS) {
        respond(c, RC_UNKNOWN_CMD, NULL, NULL);
    } else {
        if (COMMANDS[found].fn == cmd_ping)
            c->req_log = 0;                /* PING gửi định kỳ, không ghi log */
        if (COMMANDS[found].need_login && c->user == NULL)
            respond(c, RC_NOT_LOGGED_IN, NULL, NULL);
        else
            COMMANDS[found].fn(c, req);
    }
    cJSON_Delete(req);
}

void respond(Conn *c, int code, const char *msg, cJSON *data)
{
    if (!msg)
        msg = proto_default_msg(code);
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "type", "res");
    cJSON_AddNumberToObject(o, "code", code);
    cJSON_AddStringToObject(o, "msg", msg);
    if (data && code == RC_OK)
        cJSON_AddItemToObject(o, "data", data);
    else
        cJSON_Delete(data);

    char *s = cJSON_PrintUnformatted(o);
    if (s)
        conn_send_line(c, s);
    if (c->req_log)
        log_request(c, code, msg);
    free(s);
    cJSON_Delete(o);
    free(c->req_text);
    c->req_text = NULL;
}

void send_event(Conn *c, const char *evt, cJSON *data)
{
    cJSON *o = cJSON_CreateObject();
    cJSON_AddStringToObject(o, "type", "evt");
    cJSON_AddStringToObject(o, "evt", evt);
    cJSON_AddItemToObject(o, "data", data ? data : cJSON_CreateObject());
    char *s = cJSON_PrintUnformatted(o);
    if (s)
        conn_send_line(c, s);
    free(s);
    cJSON_Delete(o);
}
