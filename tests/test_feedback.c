#include "core/feedback.h"
#include "core/http.h"
#include "core/jobs.h"
#include "core/store.h"
#include "core/util.h"
#include "cJSON.h"

#ifdef NDEBUG
#error "Feedback tests require assertions enabled"
#endif
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define RECEIPT "73652d4a-60ea-4435-93fd-b22fa2609b79"
#define PREPARED "{\"upload_token\":\"test-token\",\"expires_at\":\"2099-10-21T08:05:00Z\",\"max_file_bytes\":5242880,\"max_request_bytes\":11534336}"

static int64_t now = 1000000;
static int calls, uploads;
static bm_feedback *expected;
static const char *replies[4];
static long statuses[4], retry_after;
static char first_metadata[1024];

static int64_t clock_ms(void *user) { (void)user; return now; }
static void on_upload(void *user) { (void)user; uploads++; }

static bool mock_request(const bm_http_req *req, bm_http_resp *resp, void *user) {
    (void)user;
    assert(req->cookie == NULL && req->origin == NULL && req->referer == NULL);
    assert(req->no_follow_redirect);
    assert(strcmp(req->method, "POST") == 0);
    if (strstr(req->url, "/prepare")) {
        assert(strcmp(req->url, "https://api.powerlive.io/feedback/prepare") == 0);
        assert(req->timeout_ms == 25000 && req->part_count == 0);
        assert(strcmp(req->content_type, "application/json") == 0);
        assert(req->extra_header_count == 0);
        cJSON *meta = cJSON_Parse(req->body);
        assert(meta && cJSON_GetArraySize(meta) == 6);
        assert(strcmp(cJSON_GetObjectItem(meta, "app_id")->valuestring, "bili-messenger") == 0);
        assert(strcmp(cJSON_GetObjectItem(meta, "submission_id")->valuestring, expected->submission_id) == 0);
        assert(strcmp(cJSON_GetObjectItem(meta, "app_version")->valuestring, expected->app_version) == 0);
        assert(strcmp(cJSON_GetObjectItem(meta, "os")->valuestring, expected->os) == 0);
        assert(cJSON_GetObjectItem(meta, "screenshot_size")->valueint == 0);
        assert(cJSON_GetObjectItem(meta, "log_size")->valueint
               == (int)(expected->include_log ? strlen(expected->log) : 0));
        if (first_metadata[0]) assert(strcmp(first_metadata, req->body) == 0);
        else bm_strlcpy(first_metadata, req->body, sizeof first_metadata);
        cJSON_Delete(meta);
    } else {
        assert(strcmp(req->url, "https://api.powerlive.io/feedback") == 0);
        assert(req->timeout_ms == 60000 && req->content_type == NULL && req->body == NULL);
        assert(req->extra_header_count == 1);
        assert(strcmp(req->extra_headers[0], "X-Feedback-Token: test-token") == 0);
        assert(req->part_count == 1 + (expected->email[0] ? 1 : 0) + (expected->include_log ? 1 : 0));
        assert(strcmp(req->parts[0].name, "message") == 0);
        assert(req->parts[0].size == strlen(expected->message));
        assert(strcmp(req->parts[0].data, expected->message) == 0);
        if (expected->email[0]) {
            assert(strcmp(req->parts[1].name, "email") == 0);
            assert(strcmp(req->parts[1].data, expected->email) == 0);
        }
        if (expected->include_log) {
            const bm_http_part *log = &req->parts[req->part_count - 1];
            assert(strcmp(log->name, "log") == 0);
            assert(strcmp(log->filename, "diagnostics.log") == 0);
            assert(strcmp(log->content_type, "text/plain; charset=utf-8") == 0);
            assert(log->size == strlen(expected->log) && log->size > 0);
            assert(memcmp(log->data, expected->log, log->size) == 0);
        }
    }
    assert(calls < 4);
    int index = calls++;
    resp->status = statuses[index];
    resp->retry_after_seconds = retry_after;
    resp->ok = replies[index] != NULL;
    if (resp->ok) {
        resp->data = bm_strdup(replies[index]);
        resp->size = strlen(resp->data);
    }
    return resp->ok;
}

static bm_feedback draft(void) {
    bm_feedback d = {0};
    strcpy(d.message, "中文 @foo; 问题\n第二行 😀");
    bm_feedback_freeze(&d);
    return d;
}

static void reset(bm_feedback *d) {
    calls = uploads = 0;
    retry_after = 0;
    first_metadata[0] = '\0';
    expected = d;
    for (int i = 0; i < 4; i++) { replies[i] = PREPARED; statuses[i] = 200; }
    bm_http_set_mock(mock_request, NULL);
}

static void test_validation(void) {
    bm_feedback d = {0};
    char error[384];
    assert(!bm_feedback_validate(&d, error, sizeof error));
    strcpy(d.message, "　 \t\n");
    assert(!bm_feedback_validate(&d, error, sizeof error));
    strcpy(d.message, "　 中文 😀 \n");
    strcpy(d.email, " user@example.com ");
    assert(bm_feedback_validate(&d, error, sizeof error));
    assert(strcmp(d.message, "中文 😀") == 0 && strcmp(d.email, "user@example.com") == 0);
    const char *invalid[] = {"Name <user@example.com>", "a@@b.com", "a@-b.com", "a@b..com",
                             "a b@c.com", ".a@b.com", "a.@b.com", "a@", "@b.com"};
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
        strcpy(d.email, invalid[i]);
        assert(!bm_feedback_validate(&d, error, sizeof error));
    }
    d.email[0] = '\0';
    for (int i = 0; i < 5000; i++) memcpy(d.message + i * 4, "😀", 4);
    d.message[20000] = '\0';
    assert(bm_feedback_validate(&d, error, sizeof error));
    memset(d.message, 'a', 5001); d.message[5001] = '\0';
    assert(!bm_feedback_validate(&d, error, sizeof error));
    const char *bad_utf8[] = {"\xc0\xaf", "\xed\xa0\x80", "\xf4\x90\x80\x80", "\xe4\xb8", "x\001", "x\177"};
    for (size_t i = 0; i < sizeof bad_utf8 / sizeof bad_utf8[0]; i++) {
        strcpy(d.message, bad_utf8[i]);
        assert(!bm_feedback_validate(&d, error, sizeof error));
    }
    puts("PASS Unicode code point limits, whitespace, control characters and email validation");
}

static void test_success(void) {
    for (int mode = 0; mode < 4; mode++) {
        bm_feedback d = draft();
        bm_feedback_edit(&d);
        if (mode & 1) strcpy(d.email, "test@example.com");
        d.include_log = (mode & 2) != 0;
        bm_feedback_freeze(&d);
        reset(&d);
        replies[1] = "{\"id\":\"" RECEIPT "\"}";
        statuses[1] = (mode & 1) ? 200 : 201;
        bm_feedback_result r;
        bm_feedback_send(&d, &r, on_upload, NULL);
        assert(r.ok && calls == 2 && uploads == 1);
        assert(strcmp(d.receipt, RECEIPT) == 0);
        bm_feedback_send(&d, &r, on_upload, NULL);
        assert(r.ok && calls == 2); /* No request after success. */
    }
    bm_feedback d = draft();
    reset(&d);
    replies[0] = "{\"already_submitted\":true,\"id\":\"" RECEIPT "\"}";
    bm_feedback_result r;
    bm_feedback_send(&d, &r, on_upload, NULL);
    assert(r.ok && calls == 1 && uploads == 0 && strcmp(d.receipt, RECEIPT) == 0);
    puts("PASS anonymous prepare, optional log/email multipart, 200/201 and already_submitted");
}

static bool diagnostic_mock(const bm_http_req *req, bm_http_resp *resp, void *user) {
    (void)req; (void)user;
    resp->data = bm_strdup("{\"code\":-101,\"message\":\"PRIVATE_SERVER_MESSAGE\","
                           "\"data\":{\"email\":\"PRIVATE_EMAIL\",\"cookie\":\"PRIVATE_COOKIE\"}}");
    resp->size = strlen(resp->data);
    strcpy(resp->error, "PRIVATE_ERROR");
    strcpy(resp->set_cookies[0], "PRIVATE_RESPONSE_COOKIE");
    resp->set_cookie_count = 1;
    resp->status = 200;
    resp->ok = true;
    return true;
}

static void diagnostic_job(void *unused) {
    (void)unused;
    bm_http_req req = {.url = "https://api.bilibili.com/x/web-interface/nav?secret=PRIVATE"};
    bm_http_resp resp = {.status = 500};
    for (int i = 0; i < 200; i++) bm_diagnostics_http(&req, &resp, 28, 100);
}

static void test_diagnostics(void) {
    char snapshot[BM_DIAGNOSTIC_LOG_CAP], small[8];
    assert(bm_diagnostics_snapshot(snapshot, sizeof snapshot) > 0);
    assert(strstr(snapshot, "session diagnostics"));
    const char *headers[] = {"Authorization: PRIVATE_AUTH"};
    bm_http_req req = {
        .url = "https://api.bilibili.com/x/web-interface/nav?uid=PRIVATE_UID&token=PRIVATE_TOKEN",
        .cookie = "PRIVATE_COOKIE", .method = "PRIVATE_METHOD", .body = "PRIVATE_BODY",
        .extra_headers = headers, .extra_header_count = 1
    };
    bm_http_resp resp;
    bm_http_set_mock(diagnostic_mock, NULL);
    assert(bm_http_request(&req, &resp));
    bm_http_resp_free(&resp);
    req.url = "https://PRIVATE_HOST/PRIVATE_PATH?PRIVATE_QUERY";
    assert(bm_http_request(&req, &resp));
    bm_http_resp_free(&resp);
    bm_http_set_mock(NULL, NULL);
    bm_diagnostics_snapshot(snapshot, sizeof snapshot);
    assert(strstr(snapshot, "op=account_check http=200 transport=0 api=-101"));
    assert(strstr(snapshot, "op=other_request"));
    assert(!strstr(snapshot, "PRIVATE"));
    assert(bm_diagnostics_snapshot(small, sizeof small) == 0 && small[0] == '\0');

    bm_jobs_init();
    for (int i = 0; i < 4; i++) assert(bm_job_spawn(diagnostic_job, NULL));
    int tries = 0;
    while (bm_jobs_active_count() && tries++ < 5000) {
        bm_diagnostics_snapshot(snapshot, sizeof snapshot);
        bm_sleep_ms(1);
    }
    assert(!bm_jobs_active_count());
    size_t len = bm_diagnostics_snapshot(snapshot, sizeof snapshot);
    assert(len == strlen(snapshot) && len < sizeof snapshot && snapshot[len - 1] == '\n');
    int lines = 0;
    for (const char *p = snapshot; (p = strstr(p, " op=")); p++) lines++;
    assert(lines == BM_DIAGNOSTIC_EVENTS);
    assert(!strstr(snapshot, "other_request") && !strstr(snapshot, "PRIVATE"));
    bm_jobs_shutdown();
    puts("PASS automatic diagnostic collection, secret exclusion, bounded ring and concurrent snapshots");
}

static void test_application_logs(void) {
    char snapshot[BM_DIAGNOSTIC_LOG_CAP];
    bm_diagnostics_event(BM_EVENT_APP_START, BM_LOG_INFO, 0, 0);
    bm_diagnostics_event(BM_EVENT_MESSAGE_SEND, BM_LOG_ERROR, -101, 1);
    bm_diagnostics_event(BM_EVENT_BATCH_STOP, BM_LOG_WARNING, 1, 0);
    bm_diagnostics_event((bm_log_event)-1, BM_LOG_INFO, 0, 0);
    assert(!bm_job_spawn(NULL, NULL));
    const char *path = "bm_test_PRIVATE_PATH.json";
    const char *json = "{\"roomID\":\"PRIVATE_ROOM\",\"templateList\":[\"PRIVATE_TEMPLATE\"],"
                       "\"template:PRIVATE_TEMPLATE\":\"PRIVATE_MESSAGE\",\"codes\":[\"PRIVATE_CODE\"]}";
    assert(bm_write_file(path, json, strlen(json)));
    bm_store store;
    bm_store_init(&store);
    assert(bm_store_load(&store, path));
    assert(bm_store_save(&store));
    bm_store_free(&store);
    assert(remove(path) == 0);
    char *contents = NULL;
    assert(!bm_read_file("nonexistent/PRIVATE_PATH", &contents, NULL));
    bm_diagnostics_snapshot(snapshot, sizeof snapshot);
    assert(strstr(snapshot, "[INFO] app op=app_start"));
    assert(strstr(snapshot, "[ERROR] app op=message_send code=-101 count=1"));
    assert(strstr(snapshot, "[WARN] app op=batch_stop code=1"));
    assert(strstr(snapshot, "[ERROR] app op=job_failed code=1"));
    assert(strstr(snapshot, "[INFO] app op=config_load code=0"));
    assert(strstr(snapshot, "[INFO] app op=config_save code=0"));
    assert(strstr(snapshot, "[INFO] app op=file_write code=0"));
    assert(strstr(snapshot, "[WARN] app op=file_read code=1"));
    assert(!strstr(snapshot, "PRIVATE"));
    puts("PASS application levels, actual config/file/job logging and sensitive value exclusion");
}

static void test_log_snapshot(void) {
    const char *path = "bm_test_feedback_log.json";
    bm_feedback d = {.include_log = true}, restored = {0};
    strcpy(d.message, "日志反馈");
    bm_feedback_capture_log(&d);
    assert(d.log[0]);
    char original[BM_DIAGNOSTIC_LOG_CAP];
    strcpy(original, d.log);
    bm_http_req req = {.url = "https://api.powerlive.io/feedback"};
    bm_http_resp resp = {.status = 503};
    bm_diagnostics_http(&req, &resp, 0, 10);
    bm_feedback_freeze(&d);
    assert(strcmp(original, d.log) == 0); /* What was previewed is what is sent. */
    bm_feedback_capture_log(&d);
    assert(strcmp(original, d.log) == 0); /* Frozen reports cannot refresh. */
    assert(bm_feedback_save(&d, path));
    assert(bm_feedback_load(&restored, path));
    assert(restored.include_log && !strcmp(restored.log, original));
    assert(!strcmp(restored.submission_id, d.submission_id));

    reset(&restored);
    replies[1] = "{\"error\":\"invalid_upload_token\"}";
    statuses[1] = 403;
    bm_feedback_result r;
    bm_feedback_send(&restored, &r, NULL, NULL);
    assert(!r.ok && calls == 2);
    now = restored.retry_at_ms;
    replies[3] = "{\"id\":\"" RECEIPT "\"}";
    bm_feedback_send(&restored, &r, NULL, NULL);
    assert(r.ok && calls == 4 && !strcmp(restored.log, original));

    reset(&d);
    replies[0] = "{\"already_submitted\":true,\"id\":\"" RECEIPT "\"}";
    bm_feedback_send(&d, &r, NULL, NULL);
    assert(r.ok && calls == 1); /* No attachment upload on recovered success. */
    bm_feedback_edit(&d);
    assert(d.include_log && !d.log[0]);
    bm_feedback_freeze(&d);
    assert(strcmp(d.log, original) != 0);
    reset(&d);
    replies[0] = "{\"upload_token\":\"test-token\",\"expires_at\":\"2099-01-01T00:00:00Z\","
                 "\"max_file_bytes\":1,\"max_request_bytes\":11534336}";
    bm_feedback_send(&d, &r, NULL, NULL);
    assert(!r.ok && calls == 1 && !strcmp(r.code, "request_too_large"));

    /* Missing frozen bytes must not be silently replaced by current session logs. */
    d.log[0] = '\0';
    reset(&d);
    bm_feedback_send(&d, &r, NULL, NULL);
    assert(!r.ok && calls == 0 && !strcmp(r.code, "invalid_draft"));
    assert(bm_feedback_save(&d, path));
    assert(!bm_feedback_load(&restored, path));

    /* Old text-only drafts remain text-only when retrying after an upgrade. */
    cJSON *legacy = cJSON_CreateObject();
    cJSON_AddStringToObject(legacy, "message", d.message);
    cJSON_AddStringToObject(legacy, "email", "");
    cJSON_AddStringToObject(legacy, "submission_id", d.submission_id);
    cJSON_AddStringToObject(legacy, "app_version", d.app_version);
    cJSON_AddStringToObject(legacy, "os", d.os);
    cJSON_AddStringToObject(legacy, "receipt", "");
    cJSON_AddNumberToObject(legacy, "retry_at_ms", 0);
    char *text = cJSON_PrintUnformatted(legacy);
    assert(bm_write_file(path, text, strlen(text)));
    free(text);
    assert(bm_feedback_load(&restored, path));
    assert(!restored.include_log && !restored.log[0]);
    cJSON_ReplaceItemInObject(legacy, "submission_id", cJSON_CreateString(""));
    text = cJSON_PrintUnformatted(legacy);
    assert(bm_write_file(path, text, strlen(text)));
    free(text);
    cJSON_Delete(legacy);
    assert(bm_feedback_load(&restored, path) && restored.include_log);
    assert(remove(path) == 0);
    puts("PASS preview/freeze, persisted log bytes, retry, limits, missing snapshot and legacy drafts");
}

static void test_unknown_result_and_retry(void) {
    const char *unknown[] = {NULL, "<html>proxy failure</html>", "{}", "{\"id\":\"invalid\"}",
                            "{\"error\":\"feedback_unavailable\"}", "{\"error\":\"invalid_upload_token\"}",
                            "{\"id\":\"" RECEIPT "\"}trailing garbage"};
    const long status[] = {0, 502, 201, 200, 503, 403, 201};
    for (size_t i = 0; i < sizeof unknown / sizeof unknown[0]; i++) {
        bm_feedback d = draft(), original = d;
        reset(&d);
        replies[1] = unknown[i];
        statuses[1] = status[i];
        bm_feedback_result r;
        bm_feedback_send(&d, &r, on_upload, NULL);
        assert(!r.ok && r.retryable && calls == 2);
        assert(strcmp(d.submission_id, original.submission_id) == 0);
        assert(strcmp(d.message, original.message) == 0 && !d.receipt[0]);
        bm_feedback_send(&d, &r, on_upload, NULL);
        assert(calls == 2); /* Respect cooldown, no parallel retry. */
        now = d.retry_at_ms;
        replies[2] = "{\"already_submitted\":true,\"id\":\"" RECEIPT "\"}";
        bm_feedback_send(&d, &r, on_upload, NULL);
        assert(r.ok && calls == 3 && uploads == 1);
    }
    bm_feedback d = draft();
    reset(&d);
    replies[1] = "{\"error\":\"invalid_upload_token\"}";
    statuses[1] = 403;
    bm_feedback_result r;
    bm_feedback_send(&d, &r, on_upload, NULL);
    assert(!r.ok && calls == 2);
    now = d.retry_at_ms;
    replies[3] = "{\"id\":\"" RECEIPT "\"}";
    statuses[3] = 201;
    bm_feedback_send(&d, &r, on_upload, NULL);
    assert(r.ok && calls == 4 && uploads == 2);
    puts("PASS timeout, non-JSON, invalid receipt, 503 and expired token preserve original draft on retry");
}

static void test_errors(void) {
    const char *errors[] = {"validation_error", "feedback_app_disabled", "submission_conflict",
        "submission_in_progress", "rate_limit_exceeded", "upload_busy", "upload_quota_exceeded",
        "challenge_required", "challenge_failed", "request_too_large"};
    for (size_t i = 0; i < sizeof errors / sizeof errors[0]; i++) {
        bm_feedback d = draft();
        reset(&d);
        char json[256];
        snprintf(json, sizeof json, "{\"error\":\"%s\"}", errors[i]);
        replies[0] = json; statuses[0] = 429; retry_after = 1234;
        bm_feedback_result r;
        bm_feedback_send(&d, &r, on_upload, NULL);
        assert(!r.ok && calls == 1 && uploads == 0);
        assert(strcmp(r.code, errors[i]) == 0 && r.status == 429);
        assert(d.retry_at_ms == now + 1234000);
        char id[37]; strcpy(id, d.submission_id);
        bm_feedback_edit(&d);
        bm_feedback_freeze(&d);
        assert(strcmp(id, d.submission_id) != 0 && d.retry_at_ms == now + 1234000);
        assert(r.retryable == (i != 0 && i != 1 && i != 9));
    }
    const char *bad_prepare[] = {
        "{}", "<html>bad</html>", "{\"already_submitted\":true}",
        "{\"already_submitted\":true,\"id\":\"00000000-0000-0000-0000-000000000000\"}",
        "{\"upload_token\":\"x\\r\\nInjected: true\",\"expires_at\":\"x\",\"max_file_bytes\":1,\"max_request_bytes\":99999}",
        "{\"upload_token\":\"test-token\",\"expires_at\":\"x\",\"max_file_bytes\":1,\"max_request_bytes\":1}"
    };
    for (size_t i = 0; i < sizeof bad_prepare / sizeof bad_prepare[0]; i++) {
        bm_feedback d = draft();
        reset(&d); replies[0] = bad_prepare[i];
        bm_feedback_result r;
        bm_feedback_send(&d, &r, on_upload, NULL);
        assert(!r.ok && calls == 1 && !uploads);
    }
    bm_feedback d = draft();
    reset(&d); replies[0] = NULL;
    bm_feedback_result r;
    bm_feedback_send(&d, &r, on_upload, NULL);
    assert(!r.ok && !strcmp(r.code, "network_error") && !uploads);
    puts("PASS API error codes, Retry-After, verification fallback and malformed prepare responses");
}

static void test_persistence(void) {
    const char *path = "bm_test_feedback.json";
    bm_feedback d = draft(), loaded = {0};
    strcpy(d.email, "test@example.com");
    d.retry_at_ms = now + 60000;
    assert(bm_feedback_load(&loaded, "bm_test_missing_feedback.json"));
    assert(bm_feedback_save(&d, path));
    assert(bm_feedback_load(&loaded, path));
    assert(memcmp(&d, &loaded, sizeof d) == 0);
    bm_feedback_freeze(&loaded);
    assert(strcmp(d.submission_id, loaded.submission_id) == 0);
    strcpy(d.receipt, RECEIPT);
    assert(bm_feedback_save(&d, path));
    assert(bm_feedback_load(&loaded, path) && strcmp(loaded.receipt, RECEIPT) == 0);
    char *text = NULL;
    assert(bm_read_file(path, &text, NULL));
    assert(!strstr(text, "upload_token") && !strstr(text, "SESSDATA"));
    free(text);
    assert(bm_write_file(path, "{bad", 4));
    assert(!bm_feedback_load(&loaded, path));
    assert(strcmp(loaded.receipt, RECEIPT) == 0); /* Do not discard existing state on corrupt file. */
    assert(!bm_feedback_save(&d, "nonexistent-directory/bm_feedback.json"));
    reset(&d);
    d.receipt[0] = '\0';
    d.retry_at_ms = 0;
    retry_after = LONG_MAX;
    replies[0] = "{\"error\":\"rate_limit_exceeded\"}";
    statuses[0] = 429;
    bm_feedback_result r;
    bm_feedback_send(&d, &r, NULL, NULL);
    assert(!r.ok && d.retry_at_ms > now);
    assert(bm_feedback_save(&d, path));
    assert(bm_feedback_load(&loaded, path) && loaded.retry_at_ms == d.retry_at_ms);
    assert(remove(path) == 0);
    puts("PASS persistent frozen draft, receipt, cooldown and safe handling of storage failures");
}

/* Used by test_feedback_http.py to verify the real libcurl wire format and headers. */
static int test_http_transport(const char *url) {
    bm_http_init();
    const char *message = "中文 @foo; 问题\n第二行 😀";
    const char *headers[] = {"X-Feedback-Token: local-test-token"};
    const char *log = "UTF-8 诊断日志\nop=message_send_or_recall http=200 api=-101\n";
    for (int mode = 0; mode < 4; mode++) {
        bm_http_part parts[3] = {{.name = "message", .data = message, .size = strlen(message)}};
        int count = 1;
        if (mode & 1) parts[count++] = (bm_http_part){
            .name = "email", .data = "test@example.com", .size = strlen("test@example.com")
        };
        if (mode & 2) parts[count++] = (bm_http_part){
            .name = "log", .data = log, .size = strlen(log),
            .filename = "diagnostics.log", .content_type = "text/plain; charset=utf-8"
        };
        bm_http_req req = {
            .url = url, .method = "POST", .parts = parts, .part_count = count,
            .extra_headers = headers, .extra_header_count = 1,
            .no_follow_redirect = true, .timeout_ms = 3000
        };
        bm_http_resp resp;
        assert(bm_http_request(&req, &resp));
        assert(resp.status == 429 && resp.retry_after_seconds == 600);
        bm_http_resp_free(&resp);
    }
    bm_http_cleanup();
    puts("PASS real libcurl multipart encoding and Retry-After header parsing");
    return 0;
}

int main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[1], "--http")) return test_http_transport(argv[2]);
    bm_set_time_hooks(clock_ms, NULL, NULL);
    test_diagnostics();
    test_application_logs();
    test_log_snapshot();
    test_validation();
    test_success();
    test_unknown_result_and_retry();
    test_errors();
    test_persistence();
    bm_http_set_mock(NULL, NULL);
    bm_set_time_hooks(NULL, NULL, NULL);
    puts("All feedback test suites passed!");
    return 0;
}
