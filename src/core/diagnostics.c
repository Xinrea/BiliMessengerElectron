#include "core/diagnostics.h"
#include "core/http.h"
#include "core/util.h"
#include "cJSON.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
static SRWLOCK lock = SRWLOCK_INIT;
#define LOCK() AcquireSRWLockExclusive(&lock)
#define UNLOCK() ReleaseSRWLockExclusive(&lock)
#else
#include <pthread.h>
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
#define LOCK() pthread_mutex_lock(&lock)
#define UNLOCK() pthread_mutex_unlock(&lock)
#endif

typedef struct {
    const char *operation; /* Static allowlist label, never caller-controlled text. */
    int64_t timestamp_ms, elapsed_ms;
    long status;
    int transport, api_code;
    bool has_api_code;
    size_t bytes;
    bool application;
    bm_log_level level;
    int code;
    int64_t count;
} diagnostic_event;

static diagnostic_event events[BM_DIAGNOSTIC_EVENTS];
static size_t next_event, event_count;

static void append_event(const diagnostic_event *event) {
    LOCK();
    events[next_event] = *event;
    next_event = (next_event + 1) % BM_DIAGNOSTIC_EVENTS;
    if (event_count < BM_DIAGNOSTIC_EVENTS) event_count++;
    UNLOCK();
}

void bm_diagnostics_event(bm_log_event kind, bm_log_level level, int code, int64_t count) {
    static const char *names[BM_EVENT_COUNT] = {
        "app_start", "app_stop", "config_load", "config_save", "file_read", "file_write",
        "job_start", "job_finish", "job_failed", "callback_failed",
        "account_check", "login", "room_bind", "guard_snapshot", "gift_history",
        "message_send", "message_recall", "batch_finish", "batch_stop", "feedback_result"
    };
    if (kind < 0 || kind >= BM_EVENT_COUNT || level < BM_LOG_INFO || level > BM_LOG_ERROR) return;
    diagnostic_event event = {
        .operation = names[kind], .timestamp_ms = bm_now_ms(), .application = true,
        .level = level, .code = code, .count = count
    };
    append_event(&event);
}

static const char *operation_name(const char *url) {
    static const struct { const char *url, *name; } routes[] = {
        {"https://api.bilibili.com/x/web-interface/nav", "account_check"},
        {"https://line3-h5-mobile-api.biligame.com/game/center/h5/user/space/info", "user_profile"},
        {"https://api.bilibili.com/x/web-interface/search/type", "user_search"},
        {"https://api.live.bilibili.com/xlive/web-room/v1/index/getH5InfoByRoom", "room_info"},
        {"https://api.live.bilibili.com/xlive/app-room/v2/guardTab/topList", "guard_list"},
        {"http://guard.vjoi.cn/day", "guard_calendar"},
        {"http://guard.vjoi.cn/history", "guard_history"},
        {"https://api.vc.bilibili.com/web_im/v1/web_im/send_msg", "message_send_or_recall"},
        {"https://api.live.bilibili.com/xlive/revenue/v1/giftStream/getReceivedGiftStream", "gift_history"},
        {"https://passport.bilibili.com/x/passport-login/web/qrcode/generate", "login_qr_generate"},
        {"https://passport.bilibili.com/x/passport-login/web/qrcode/poll", "login_qr_poll"},
        {"https://api.github.com/repos/powerlive-io/bili-messenger/releases/latest", "update_check"},
        {"https://api.powerlive.io/feedback/prepare", "feedback_prepare"},
        {"https://api.powerlive.io/feedback", "feedback_upload"}
    };
    if (!url) return "unknown_request";
    for (size_t i = 0; i < sizeof routes / sizeof routes[0]; i++) {
        size_t n = strlen(routes[i].url);
        if (!strncmp(url, routes[i].url, n) && (!url[n] || url[n] == '?' || url[n] == '#'))
            return routes[i].name;
    }
    if (!strncmp(url, "https://api.vtbs.moe/v2/", strlen("https://api.vtbs.moe/v2/")))
        return "room_trend";
    return "other_request"; /* Includes avatars and login redirects; do not retain paths. */
}

void bm_diagnostics_http(const bm_http_req *req, const bm_http_resp *resp,
                         int transport_code, int64_t elapsed_ms) {
    diagnostic_event event = {
        .operation = operation_name(req ? req->url : NULL),
        .timestamp_ms = bm_now_ms(), .elapsed_ms = elapsed_ms < 0 ? 0 : elapsed_ms,
        .status = resp->status, .transport = transport_code, .bytes = resp->size
    };
    /* Bilibili often reports business errors with HTTP 200. Retain only the
       numeric top-level code; never its message, data or any feedback error text.
       Bound parsing work so image downloads/large lists do not incur JSON work. */
    if (resp->data && resp->size <= 65536 && resp->data[0] == '{') {
        cJSON *json = cJSON_Parse(resp->data);
        const cJSON *code = cJSON_GetObjectItemCaseSensitive(json, "code");
        if (cJSON_IsNumber(code) && code->valuedouble >= INT_MIN && code->valuedouble <= INT_MAX
            && code->valuedouble == (double)code->valueint) {
            event.has_api_code = true;
            event.api_code = code->valueint;
        }
        cJSON_Delete(json);
    }
    append_event(&event);
}

size_t bm_diagnostics_snapshot(char *out, size_t cap) {
    if (!cap) return 0;
    size_t pos = 0;
    out[0] = '\0';
    const char *header = "BiliMessenger session diagnostics v2\n"
        "Latest application and HTTP logs (oldest first, maximum 256).\n"
        "Times: Unix milliseconds. transport: libcurl code (0=OK, -1=mock failure).\n"
        "Application code: 0=success unless noted; count: records/bytes, not an identifier.\n"
        "No request parameters, headers, bodies, account IDs or raw error text collected.\n";
    if (strlen(header) >= cap) return 0;
    bm_strlcpy(out, header, cap);
    pos = strlen(out);
    LOCK();
    for (size_t i = 0; i < event_count; i++) {
        const diagnostic_event *e = &events[(next_event + BM_DIAGNOSTIC_EVENTS - event_count + i)
                                            % BM_DIAGNOSTIC_EVENTS];
        char line[256], api[32];
        if (e->has_api_code) snprintf(api, sizeof api, "%d", e->api_code);
        else bm_strlcpy(api, "n/a", sizeof api);
        static const char *levels[] = {"INFO", "WARN", "ERROR"};
        int n;
        if (e->application) {
            n = snprintf(line, sizeof line, "%lld [%s] app op=%s code=%d count=%lld\n",
                         (long long)e->timestamp_ms, levels[e->level], e->operation,
                         e->code, (long long)e->count);
        } else {
            const char *level = e->transport || e->status >= 400 || (e->has_api_code && e->api_code)
                ? "WARN" : "INFO";
            n = snprintf(line, sizeof line,
                         "%lld [%s] http op=%s http=%ld transport=%d api=%s elapsed_ms=%lld bytes=%zu\n",
                         (long long)e->timestamp_ms, level, e->operation, e->status, e->transport,
                         api, (long long)e->elapsed_ms, e->bytes);
        }
        if (n < 0 || (size_t)n >= sizeof line || (size_t)n >= cap - pos) break;
        memcpy(out + pos, line, (size_t)n + 1);
        pos += (size_t)n;
    }
    UNLOCK();
    return pos;
}
