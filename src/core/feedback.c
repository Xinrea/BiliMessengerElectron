#include "core/feedback.h"
#include "core/http.h"
#include "core/util.h"
#include "cJSON.h"

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#define FEEDBACK_URL "https://api.powerlive.io/feedback"

static bool uuid_valid(const char *s) {
    if (!s || strlen(s) != 36) return false;
    bool nonzero = false;
    for (int i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (s[i] != '-') return false;
        } else {
            if (!isxdigit((unsigned char)s[i])) return false;
            if (s[i] != '0') nonzero = true;
        }
    }
    return nonzero;
}

/* Strict UTF-8 decoding rejects overlong encodings, surrogates and truncation. */
static bool next_codepoint(const unsigned char **cursor, unsigned *cp) {
    const unsigned char *s = *cursor;
    unsigned c = *s++, n = 0, min = 0;
    if (c < 0x80) { *cp = c; *cursor = s; return true; }
    if (c >= 0xc2 && c <= 0xdf) { c &= 31; n = 1; min = 0x80; }
    else if (c >= 0xe0 && c <= 0xef) { c &= 15; n = 2; min = 0x800; }
    else if (c >= 0xf0 && c <= 0xf4) { c &= 7; n = 3; min = 0x10000; }
    else return false;
    for (unsigned i = 0; i < n; i++) {
        if ((*s & 0xc0) != 0x80) return false;
        c = (c << 6) | (*s++ & 63);
    }
    if (c < min || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return false;
    *cp = c;
    *cursor = s;
    return true;
}

static bool unicode_space(unsigned c) {
    return (c >= 9 && c <= 13) || c == 32 || c == 0x85 || c == 0xa0
        || c == 0x1680 || (c >= 0x2000 && c <= 0x200a) || c == 0x2028
        || c == 0x2029 || c == 0x202f || c == 0x205f || c == 0x3000;
}

static bool validation_error(char *error, size_t cap, const char *message) {
    bm_strlcpy(error, message, cap);
    return false;
}

bool bm_feedback_validate(bm_feedback *d, char *error, size_t cap) {
    const unsigned char *p = (const unsigned char *)d->message;
    const unsigned char *first = NULL, *last = p;
    int count = 0, trimmed_count = 0;
    while (*p) {
        const unsigned char *start = p;
        unsigned cp;
        if (!next_codepoint(&p, &cp))
            return validation_error(error, cap, "问题描述包含无效 UTF-8 字符，请重新输入。");
        if ((cp < 32 && cp != 9 && cp != 10 && cp != 13) || (cp >= 127 && cp <= 159))
            return validation_error(error, cap, "问题描述不能包含控制字符。");
        if (!unicode_space(cp)) {
            if (!first) first = start;
            last = p;
            trimmed_count = ++count;
        } else if (first) count++;
    }
    if (!first || trimmed_count > 5000)
        return validation_error(error, cap, "请填写 1–5000 个字符的问题描述（去除首尾空白）。");
    size_t len = (size_t)(last - first);
    memmove(d->message, first, len);
    d->message[len] = '\0';

    char *email = bm_trim(d->email);
    memmove(d->email, email, strlen(email) + 1);
    if (d->email[0]) {
        const char *at = strchr(d->email, '@');
        bool valid = at && at != d->email && at[1] && !strchr(at + 1, '@');
        for (const unsigned char *s = (const unsigned char *)d->email; *s; s++)
            if (*s <= 32 || *s >= 127 || strchr("<>()[]\\,;:\"", *s)) valid = false;
        if (valid) {
            valid = d->email[0] != '.' && at[-1] != '.' && !strstr(d->email, "..");
            const char *label = at + 1;
            for (const char *s = label; ; s++) {
                if (*s == '.' || !*s) {
                    if (s == label || *label == '-' || s[-1] == '-') valid = false;
                    label = s + 1;
                    if (!*s) break;
                } else if (!isalnum((unsigned char)*s) && *s != '-') valid = false;
            }
        }
        if (!valid) return validation_error(error, cap, "请填写有效邮箱地址（如 user@example.com），或留空。");
    }
    if (cap) error[0] = '\0';
    return true;
}

void bm_feedback_capture_log(bm_feedback *d) {
    if (d->submission_id[0]) return;
    if (d->include_log) bm_diagnostics_snapshot(d->log, sizeof d->log);
    else d->log[0] = '\0';
}

void bm_feedback_freeze(bm_feedback *d) {
    if (d->submission_id[0]) return;
    if (!d->include_log || !d->log[0]) bm_feedback_capture_log(d);
    bm_uuid_v4(d->submission_id, sizeof d->submission_id, false);
    bm_strlcpy(d->app_version, BM_VERSION, sizeof d->app_version);
#if defined(_WIN32)
    const char *os = "Windows";
#elif defined(__APPLE__)
    const char *os = "macOS";
#else
    const char *os = "Linux";
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
    const char *arch = "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    const char *arch = "x64";
#else
    const char *arch = "other";
#endif
    snprintf(d->os, sizeof d->os, "%s %s", os, arch);
}

void bm_feedback_edit(bm_feedback *d) {
    d->submission_id[0] = d->app_version[0] = d->os[0] = d->receipt[0] = '\0';
    d->log[0] = '\0';
    /* Editing does not bypass a source-level Retry-After deadline. */
}

static const char *json_string(const cJSON *json, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(json, key);
    return cJSON_IsString(item) ? item->valuestring : "";
}

static void fail(bm_feedback_result *r, const char *code, const char *message, bool retryable) {
    bm_strlcpy(r->code, code, sizeof r->code);
    bm_strlcpy(r->message, message, sizeof r->message);
    r->retryable = retryable;
}

static void response_error(bm_feedback *d, bm_feedback_result *r,
                           const bm_http_resp *resp, const cJSON *json) {
    r->status = resp->status;
    const char *code = json_string(json, "error");
    if (!resp->ok) {
        fail(r, "network_error", "连接失败或超时，提交结果尚未确认。请保持原网络，稍后重试以确认原反馈。", true);
    } else if (!strcmp(code, "feedback_app_disabled")) {
        fail(r, code, "反馈入口尚未启用或已停用，请联系维护者检查 bili-messenger 的后台配置。", false);
    } else if (!strcmp(code, "validation_error") || !strcmp(code, "request_too_large")) {
        fail(r, code, "反馈内容未通过校验，请修改内容后作为新反馈提交。", false);
    } else if (!strcmp(code, "submission_conflict")) {
        fail(r, code, "反馈编号与原内容或网络出口冲突。请恢复原网络后确认；如需新提交，请明确选择修改为新反馈。", true);
    } else if (!strcmp(code, "challenge_required") || !strcmp(code, "challenge_failed")) {
        fail(r, code, "服务要求人机验证，当前原生客户端无法展示验证网页。请稍后再试，草稿已保留。", true);
    } else if (!strcmp(code, "invalid_upload_token")) {
        fail(r, code, "临时凭证已失效，请保持网络稳定后重试，将使用原编号重新申请凭证。", true);
    } else if (!strcmp(code, "submission_in_progress")) {
        fail(r, code, "原反馈仍在处理中，请等待后重试确认，不要另建反馈。", true);
    } else if (!strcmp(code, "rate_limit_exceeded") || !strcmp(code, "upload_busy")
               || !strcmp(code, "upload_quota_exceeded")) {
        fail(r, code, "提交频率、并发或当日额度受限，请等待后重试。新建反馈不会解除限制。", true);
    } else if (!strcmp(code, "feedback_unavailable")) {
        fail(r, code, "反馈服务暂时不可用，结果尚未确认。请稍后重试原反馈。", true);
    } else {
        fail(r, resp->status == 200 || resp->status == 201 ? "invalid_response" : "http_error",
             "未收到有效回执，结果尚未确认。草稿及原编号已保留，请稍后重试。", true);
    }
    long seconds = resp->retry_after_seconds;
    if (r->retryable && seconds < 10) seconds = 10;
    if ((!strcmp(code, "challenge_required") || !strcmp(code, "challenge_failed")) && seconds < 600)
        seconds = 600;
    int64_t now = bm_now_ms();
    /* Bound untrusted arithmetic to 15 digits so cJSON also round-trips it exactly. */
    const int64_t max_deadline = 999999999999999LL;
    d->retry_at_ms = seconds > (max_deadline - now) / 1000 ? max_deadline : now + (int64_t)seconds * 1000;
}

static cJSON *response_json(const bm_http_resp *resp) {
    if (!resp->data || strlen(resp->data) != resp->size) return NULL;
    return cJSON_ParseWithOpts(resp->data, NULL, true);
}

static bool receipt(bm_feedback *d, bm_feedback_result *r, const cJSON *json) {
    const char *id = json_string(json, "id");
    if (!uuid_valid(id)) return false;
    bm_strlcpy(d->receipt, id, sizeof d->receipt);
    d->retry_at_ms = 0;
    r->ok = true;
    bm_strlcpy(r->message, "反馈已收到", sizeof r->message);
    return true;
}

void bm_feedback_send(bm_feedback *d, bm_feedback_result *r,
                      void (*on_upload)(void *), void *user) {
    memset(r, 0, sizeof *r);
    if (d->receipt[0]) { r->ok = true; return; }
    if (!uuid_valid(d->submission_id)) {
        fail(r, "invalid_draft", "请先保存并固定反馈草稿。", false);
        return;
    }
    size_t log_size = d->include_log ? strlen(d->log) : 0;
    if (d->include_log && !log_size) {
        fail(r, "invalid_draft", "原反馈日志快照缺失，请修改为新反馈后重新采集。", false);
        return;
    }
    if (bm_now_ms() < d->retry_at_ms) {
        fail(r, "retry_later", "请等待重试倒计时结束。", true);
        return;
    }
    cJSON *meta = cJSON_CreateObject();
    bool built = meta && cJSON_AddStringToObject(meta, "app_id", "bili-messenger")
        && cJSON_AddStringToObject(meta, "submission_id", d->submission_id)
        && cJSON_AddStringToObject(meta, "app_version", d->app_version)
        && cJSON_AddStringToObject(meta, "os", d->os)
        && cJSON_AddNumberToObject(meta, "screenshot_size", 0)
        && cJSON_AddNumberToObject(meta, "log_size", (double)log_size);
    char *body = built ? cJSON_PrintUnformatted(meta) : NULL;
    cJSON_Delete(meta);
    if (!body) { fail(r, "out_of_memory", "内存不足，请稍后重试。", true); return; }
    bm_http_req req = {
        .url = FEEDBACK_URL "/prepare", .method = "POST", .body = body,
        .content_type = "application/json", .accept = "application/json",
        .timeout_ms = 25000, .no_follow_redirect = true
    };
    bm_http_resp resp;
    bm_http_request(&req, &resp);
    free(body);
    cJSON *json = response_json(&resp);
    if (resp.ok && resp.status == 200
        && cJSON_IsTrue(cJSON_GetObjectItemCaseSensitive(json, "already_submitted"))) {
        if (!receipt(d, r, json)) response_error(d, r, &resp, json);
        cJSON_Delete(json);
        bm_http_resp_free(&resp);
        return;
    }
    const char *token = json_string(json, "upload_token");
    const cJSON *max_file = cJSON_GetObjectItemCaseSensitive(json, "max_file_bytes");
    const cJSON *max_request = cJSON_GetObjectItemCaseSensitive(json, "max_request_bytes");
    bool prepared = resp.ok && resp.status == 200 && token[0] && strlen(token) <= 2048
        && !strchr(token, '\r') && !strchr(token, '\n')
        && json_string(json, "expires_at")[0]
        && cJSON_IsNumber(max_file) && max_file->valuedouble >= 0
        && cJSON_IsNumber(max_request) && max_request->valuedouble > 0;
    if (!prepared) {
        response_error(d, r, &resp, json);
        cJSON_Delete(json);
        bm_http_resp_free(&resp);
        return;
    }
    /* Conservative allowance for libcurl's generated boundary and form headers. */
    if (max_file->valuedouble < log_size
        || max_request->valuedouble < strlen(d->message) + strlen(d->email) + log_size + 2048) {
        fail(r, "request_too_large", "内容超出服务当前大小限制，请缩短文字或取消附带日志后作为新反馈提交。", false);
        cJSON_Delete(json);
        bm_http_resp_free(&resp);
        return;
    }
    char header[2080];
    snprintf(header, sizeof header, "X-Feedback-Token: %s", token);
    cJSON_Delete(json);
    bm_http_resp_free(&resp);
    const char *headers[] = { header };
    bm_http_part parts[3] = {
        { .name = "message", .data = d->message, .size = strlen(d->message) }
    };
    int part_count = 1;
    if (d->email[0])
        parts[part_count++] = (bm_http_part){ .name = "email", .data = d->email, .size = strlen(d->email) };
    if (log_size)
        parts[part_count++] = (bm_http_part){
            .name = "log", .data = d->log, .size = log_size,
            .filename = "diagnostics.log", .content_type = "text/plain; charset=utf-8"
        };
    req = (bm_http_req){
        .url = FEEDBACK_URL, .method = "POST", .accept = "application/json",
        .extra_headers = headers, .extra_header_count = 1,
        .parts = parts, .part_count = part_count,
        .timeout_ms = 60000, .no_follow_redirect = true
    };
    if (on_upload) on_upload(user);
    bm_http_request(&req, &resp);
    json = response_json(&resp);
    if (!(resp.ok && (resp.status == 200 || resp.status == 201) && receipt(d, r, json)))
        response_error(d, r, &resp, json);
    cJSON_Delete(json);
    bm_http_resp_free(&resp);
}

bool bm_feedback_save(const bm_feedback *d, const char *path) {
    cJSON *json = cJSON_CreateObject();
    bool ok = json
        && cJSON_AddStringToObject(json, "message", d->message)
        && cJSON_AddStringToObject(json, "email", d->email)
        && cJSON_AddStringToObject(json, "submission_id", d->submission_id)
        && cJSON_AddStringToObject(json, "app_version", d->app_version)
        && cJSON_AddStringToObject(json, "os", d->os)
        && cJSON_AddStringToObject(json, "receipt", d->receipt)
        && cJSON_AddBoolToObject(json, "include_log", d->include_log)
        && cJSON_AddStringToObject(json, "log", d->log)
        && cJSON_AddNumberToObject(json, "retry_at_ms", (double)d->retry_at_ms);
    char *text = ok ? cJSON_PrintUnformatted(json) : NULL;
    cJSON_Delete(json);
    if (!text) return false;
    char tmp[1200];
    int n = snprintf(tmp, sizeof tmp, "%s.tmp", path);
    if (n < 0 || (size_t)n >= sizeof tmp) { free(text); return false; }
#ifdef _WIN32
    FILE *f = fopen(tmp, "wb");
#else
    int fd = open(tmp, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    FILE *f = NULL;
    if (fd >= 0) {
        if (fchmod(fd, 0600) == 0) f = fdopen(fd, "wb");
        if (!f) close(fd);
    }
#endif
    ok = false;
    if (f) {
        size_t len = strlen(text);
        ok = fwrite(text, 1, len, f) == len;
        if (fflush(f) != 0) ok = false;
#ifndef _WIN32
        if (fsync(fileno(f)) != 0) ok = false;
#endif
        if (fclose(f) != 0) ok = false;
#ifdef _WIN32
        if (ok) ok = MoveFileExA(tmp, path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
        if (ok) ok = rename(tmp, path) == 0;
#endif
    }
    if (!ok) remove(tmp);
    free(text);
    return ok;
}

static bool load_string(const cJSON *json, const char *key, char *out, size_t cap) {
    const cJSON *v = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!cJSON_IsString(v) || strlen(v->valuestring) >= cap) return false;
    bm_strlcpy(out, v->valuestring, cap);
    return true;
}

bool bm_feedback_load(bm_feedback *d, const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return errno == ENOENT;
    /* Bound local input too; do not silently replace corrupt frozen drafts. */
    const size_t cap = 128001 + BM_DIAGNOSTIC_LOG_CAP * 6; /* Worst-case JSON escaping. */
    char *buf = (char *)malloc(cap);
    if (!buf) { fclose(f); return false; }
    size_t len = fread(buf, 1, cap - 1, f);
    bool ok = !ferror(f) && feof(f);
    fclose(f);
    if (!ok) { free(buf); return false; }
    buf[len] = '\0';
    cJSON *json = cJSON_ParseWithOpts(buf, NULL, true);
    free(buf);
    bm_feedback loaded = {0};
    ok = json
        && load_string(json, "message", loaded.message, sizeof loaded.message)
        && load_string(json, "email", loaded.email, sizeof loaded.email)
        && load_string(json, "submission_id", loaded.submission_id, sizeof loaded.submission_id)
        && load_string(json, "app_version", loaded.app_version, sizeof loaded.app_version)
        && load_string(json, "os", loaded.os, sizeof loaded.os)
        && load_string(json, "receipt", loaded.receipt, sizeof loaded.receipt);
    const cJSON *include_log = cJSON_GetObjectItemCaseSensitive(json, "include_log");
    const cJSON *log = cJSON_GetObjectItemCaseSensitive(json, "log");
    if (!include_log && !log) {
        /* Legacy frozen text-only reports must keep their original zero log_size. */
        loaded.include_log = !loaded.submission_id[0];
    } else {
        loaded.include_log = cJSON_IsTrue(include_log);
        ok = ok && cJSON_IsBool(include_log) && load_string(json, "log", loaded.log, sizeof loaded.log)
            && (loaded.include_log || !loaded.log[0])
            && (!loaded.submission_id[0] || !loaded.include_log || loaded.log[0]);
        const unsigned char *p = (const unsigned char *)loaded.log;
        while (ok && *p) {
            unsigned cp;
            ok = next_codepoint(&p, &cp);
            if (ok && ((cp < 32 && cp != 9 && cp != 10 && cp != 13) || (cp >= 127 && cp <= 159)))
                ok = false;
        }
    }
    const cJSON *retry = cJSON_GetObjectItemCaseSensitive(json, "retry_at_ms");
    ok = ok && cJSON_IsNumber(retry) && isfinite(retry->valuedouble)
        && retry->valuedouble >= 0 && retry->valuedouble < (double)INT64_MAX
        && (!loaded.submission_id[0] || uuid_valid(loaded.submission_id))
        && (!loaded.receipt[0] || (uuid_valid(loaded.receipt) && loaded.submission_id[0]));
    if (ok) {
        loaded.retry_at_ms = (int64_t)retry->valuedouble;
        *d = loaded;
    }
    cJSON_Delete(json);
    return ok;
}
