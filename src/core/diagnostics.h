#ifndef BM_DIAGNOSTICS_H
#define BM_DIAGNOSTICS_H

#include <stddef.h>
#include <stdint.h>

#define BM_DIAGNOSTIC_EVENTS 256
#define BM_DIAGNOSTIC_LOG_CAP 65536

struct bm_http_req;
struct bm_http_resp;

typedef enum {
    BM_LOG_INFO, BM_LOG_WARNING, BM_LOG_ERROR
} bm_log_level;

typedef enum {
    BM_EVENT_APP_START, BM_EVENT_APP_STOP,
    BM_EVENT_CONFIG_LOAD, BM_EVENT_CONFIG_SAVE,
    BM_EVENT_FILE_READ, BM_EVENT_FILE_WRITE,
    BM_EVENT_JOB_START, BM_EVENT_JOB_FINISH, BM_EVENT_JOB_FAILED, BM_EVENT_CALLBACK_FAILED,
    BM_EVENT_ACCOUNT_CHECK, BM_EVENT_LOGIN, BM_EVENT_ROOM_BIND,
    BM_EVENT_GUARD_SNAPSHOT, BM_EVENT_GIFT_HISTORY,
    BM_EVENT_MESSAGE_SEND, BM_EVENT_MESSAGE_RECALL, BM_EVENT_BATCH_FINISH, BM_EVENT_BATCH_STOP,
    BM_EVENT_FEEDBACK_RESULT,
    BM_EVENT_COUNT
} bm_log_event;

/* Structured application logs. code is a numeric error/status code; count is a
   quantity (records/bytes), never an identifier. No arbitrary strings accepted. */
void bm_diagnostics_event(bm_log_event event, bm_log_level level, int code, int64_t count);

/* Thread-safe, bounded session diagnostics. Only allowlisted operation labels and
   numeric fields are retained, never URLs, headers, bodies or error strings. */
void bm_diagnostics_http(const struct bm_http_req *req, const struct bm_http_resp *resp,
                         int transport_code, int64_t elapsed_ms);
/* Produces a fixed UTF-8 text snapshot, including a header even with no events. */
size_t bm_diagnostics_snapshot(char *out, size_t cap);

#endif
