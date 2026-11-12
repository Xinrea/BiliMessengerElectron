#include "core/bili.h"
#include "core/guards.h"
#include "core/http.h"
#include "core/md5.h"
#include "core/store.h"
#include "core/util.h"
#include "core/version.h"

#ifdef NDEBUG
#error "bm_tests requires assertions enabled in every build configuration"
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char url[512];
    char method[16];
    char body[2048];
    char cookie[1024];
    char origin[256];
    char referer[256];
    char user_agent[256];
    char content_type[128];
} recorded_req;

typedef struct {
    recorded_req reqs[64];
    int req_count;
    int delays[64];
    int delay_count;
    int64_t now_ms;
    const char *(*reply_fn)(const recorded_req *r, int index, bm_http_resp *resp, void *ctx);
    void *ctx;
} test_harness;

static test_harness g_h;

static int64_t mock_now_ms(void *user) {
    (void)user;
    return g_h.now_ms;
}

static void mock_sleep_ms(int ms, void *user) {
    (void)user;
    if (g_h.delay_count < 64) {
        g_h.delays[g_h.delay_count++] = ms;
    }
    g_h.now_ms += ms;
}

static bool mock_http_handler(const bm_http_req *req, bm_http_resp *resp, void *user) {
    (void)user;
    int idx = g_h.req_count;
    if (g_h.req_count < 64) {
        recorded_req *rec = &g_h.reqs[g_h.req_count++];
        bm_strlcpy(rec->url, req->url ? req->url : "", sizeof rec->url);
        bm_strlcpy(rec->method, req->method ? req->method : "GET", sizeof rec->method);
        bm_strlcpy(rec->body, req->body ? req->body : "", sizeof rec->body);
        bm_strlcpy(rec->cookie, req->cookie ? req->cookie : "", sizeof rec->cookie);
        bm_strlcpy(rec->origin, req->origin ? req->origin : "", sizeof rec->origin);
        bm_strlcpy(rec->referer, req->referer ? req->referer : "", sizeof rec->referer);
        bm_strlcpy(rec->user_agent, req->user_agent ? req->user_agent : "", sizeof rec->user_agent);
        bm_strlcpy(rec->content_type, req->content_type ? req->content_type : "", sizeof rec->content_type);
    }

    const recorded_req *r = &g_h.reqs[idx < 64 ? idx : 63];
    const char *body = g_h.reply_fn ? g_h.reply_fn(r, idx, resp, g_h.ctx) : "{\"code\":0,\"data\":{\"ready\":1,\"list\":[],\"total_page\":0}}";
    if (!body) {
        return false;
    }
    resp->ok = true;
    resp->status = 200;
    resp->data = bm_strdup(body);
    resp->size = strlen(resp->data);
    return true;
}

static void reset_harness(const char *(*fn)(const recorded_req *r, int index, bm_http_resp *resp, void *ctx), void *ctx) {
    memset(&g_h, 0, sizeof g_h);
    g_h.reply_fn = fn;
    g_h.ctx = ctx;
    bm_bili_reset_caches();
    bm_set_time_hooks(mock_now_ms, mock_sleep_ms, NULL);
    bm_http_set_mock(mock_http_handler, NULL);
}

static bm_login_info make_test_login(void) {
    bm_login_info l;
    memset(&l, 0, sizeof l);
    l.has_login = true;
    bm_strlcpy(l.SESSDATA, "test-session", sizeof l.SESSDATA);
    bm_strlcpy(l.DedeUserID, "123", sizeof l.DedeUserID);
    bm_strlcpy(l.bili_jct, "test-csrf", sizeof l.bili_jct);
    bm_strlcpy(l.buvid3, "test-device", sizeof l.buvid3);
    bm_strlcpy(l.dev_id, "TEST-DEV-ID", sizeof l.dev_id);
    return l;
}

/* --- Gift Stream Tests (ported from tests/gift-stream.test.js) --- */

static const char *reply_three_levels(const recorded_req *r, int index, bm_http_resp *resp, void *ctx) {
    (void)r; (void)index; (void)resp; (void)ctx;
    return "{\"code\":0,\"data\":{\"ready\":1,\"total_page\":1,\"list\":["
           "{\"goods_id\":5,\"uid\":5,\"uname\":\"test\",\"name\":\"purchase\",\"time\":\"2026-08-01 12:00:00\"},"
           "{\"goods_id\":6,\"uid\":6,\"uname\":\"test\",\"name\":\"purchase\",\"time\":\"2026-08-01 12:00:00\"},"
           "{\"goods_id\":7,\"uid\":7,\"uname\":\"test\",\"name\":\"purchase\",\"time\":\"2026-08-01 12:00:00\"}"
           "]}}";
}

static void test_gift_stream_levels_and_headers(void) {
    reset_harness(reply_three_levels, NULL);
    bm_login_info login = make_test_login();
    bm_guard_list guards;
    bm_guard_list_init(&guards);
    bm_bili_err err = { 0 };

    bool ok = bm_bili_get_received_guards_by_period(&login, "2026-08-01", "2026-08-31", &guards, &err);
    assert(ok);
    assert(guards.count == 3);
    assert(guards.items[0].guard_level == 1 && strcmp(bm_guard_level_str(&guards.items[0]), "总督") == 0);
    assert(guards.items[1].guard_level == 2 && strcmp(bm_guard_level_str(&guards.items[1]), "提督") == 0);
    assert(guards.items[2].guard_level == 3 && strcmp(bm_guard_level_str(&guards.items[2]), "舰长") == 0);

    assert(g_h.req_count == 1);
    assert(strcmp(g_h.reqs[0].url, "https://api.live.bilibili.com/xlive/revenue/v1/giftStream/getReceivedGiftStream") == 0);
    assert(strcmp(g_h.reqs[0].method, "POST") == 0);
    assert(strcmp(g_h.reqs[0].content_type, "application/x-www-form-urlencoded") == 0);
    assert(strcmp(g_h.reqs[0].origin, "https://link.bilibili.com") == 0);
    assert(strcmp(g_h.reqs[0].referer, "https://link.bilibili.com/p/center/index") == 0);
    assert(strstr(g_h.reqs[0].user_agent, "Chrome/154") != NULL);
    assert(strstr(g_h.reqs[0].cookie, "SESSDATA=test-session") != NULL);
    assert(strcmp(g_h.reqs[0].body, "page=0&gift_id=0&begin_date=20260801&end_date=20260831&uname=&goods_id=5%2C6%2C7&csrf_token=test-csrf&csrf=test-csrf") == 0);

    bm_guard_list_free(&guards);
    printf("PASS posts the new guard filters and adapts all three guard levels\n");
}

static const char *reply_three_pages(const recorded_req *r, int index, bm_http_resp *resp, void *ctx) {
    (void)r; (void)resp; (void)ctx;
    static char buf[256];
    snprintf(buf, sizeof buf,
             "{\"code\":0,\"data\":{\"ready\":1,\"total_page\":%d,\"list\":[{\"goods_id\":7,\"uid\":123,\"uname\":\"u\",\"time\":\"page-%d\"}]}}",
             index == 0 ? 3 : 0, index);
    return buf;
}

static void test_gift_stream_pagination(void) {
    reset_harness(reply_three_pages, NULL);
    bm_login_info login = make_test_login();
    bm_guard_list guards;
    bm_guard_list_init(&guards);
    bm_bili_err err = { 0 };

    assert(bm_bili_get_received_guards_by_period(&login, "2026-08-01", "2026-08-31", &guards, &err));
    assert(g_h.req_count == 3);
    assert(strstr(g_h.reqs[0].body, "page=0&") != NULL);
    assert(strstr(g_h.reqs[1].body, "page=1&") != NULL);
    assert(strstr(g_h.reqs[2].body, "page=2&") != NULL);
    assert(guards.count == 3);
    assert(strcmp(guards.items[0].time, "page-0") == 0);
    assert(strcmp(guards.items[1].time, "page-1") == 0);
    assert(strcmp(guards.items[2].time, "page-2") == 0);

    bm_guard_list_free(&guards);
    printf("PASS fetches every zero-based page and preserves repeated purchases\n");
}

static void test_gift_stream_leap_months(void) {
    reset_harness(NULL, NULL);
    bm_login_info login = make_test_login();
    bm_guard_list guards;
    bm_guard_list_init(&guards);
    bm_bili_err err = { 0 };

    assert(bm_bili_get_received_guards_by_period(&login, "2023-12-31", "2024-03-01", &guards, &err));
    assert(guards.count == 0);
    assert(g_h.req_count == 4);
    assert(strstr(g_h.reqs[0].body, "begin_date=20231231&end_date=20231231") != NULL);
    assert(strstr(g_h.reqs[1].body, "begin_date=20240101&end_date=20240131") != NULL);
    assert(strstr(g_h.reqs[2].body, "begin_date=20240201&end_date=20240229") != NULL);
    assert(strstr(g_h.reqs[3].body, "begin_date=20240301&end_date=20240301") != NULL);

    bm_guard_list_free(&guards);
    printf("PASS splits inclusive ranges across years and leap months\n");
}

static void test_gift_stream_single_day(void) {
    reset_harness(NULL, NULL);
    bm_login_info login = make_test_login();
    bm_guard_list guards;
    bm_guard_list_init(&guards);
    bm_bili_err err = { 0 };

    assert(bm_bili_get_received_guards_by_period(&login, "2026-10-06", "2026-10-06", &guards, &err));
    assert(g_h.req_count == 1);
    assert(strstr(g_h.reqs[0].body, "begin_date=20261006&end_date=20261006") != NULL);

    bm_guard_list_free(&guards);
    printf("PASS accepts a single-day period\n");
}

static const char *reply_pending_twice(const recorded_req *r, int index, bm_http_resp *resp, void *ctx) {
    (void)r; (void)resp; (void)ctx;
    if (index < 2) return "{\"code\":0,\"data\":{\"ready\":0}}";
    return "{\"code\":0,\"data\":{\"ready\":1,\"total_page\":1,\"list\":[{\"goods_id\":7,\"uid\":1}]}}";
}

static void test_gift_stream_retries(void) {
    reset_harness(reply_pending_twice, NULL);
    bm_login_info login = make_test_login();
    bm_guard_list guards;
    bm_guard_list_init(&guards);
    bm_bili_err err = { 0 };

    assert(bm_bili_get_received_guards_by_period(&login, "2026-08-01", "2026-08-31", &guards, &err));
    assert(guards.count == 1);
    assert(g_h.delay_count == 2 && g_h.delays[0] == 1000 && g_h.delays[1] == 2000);
    assert(g_h.req_count == 3);
    for (int i = 0; i < 3; i++) {
        assert(strstr(g_h.reqs[i].body, "page=0&") != NULL);
    }

    bm_guard_list_free(&guards);
    printf("PASS retries pending results without advancing the page\n");
}

static const char *reply_always_pending(const recorded_req *r, int index, bm_http_resp *resp, void *ctx) {
    (void)r; (void)index; (void)resp; (void)ctx;
    return "{\"code\":0,\"data\":{\"ready\":0}}";
}

static void test_gift_stream_timeout_bounds(void) {
    reset_harness(reply_always_pending, NULL);
    bm_login_info login = make_test_login();
    bm_guard_list guards;
    bm_guard_list_init(&guards);
    bm_bili_err err = { 0 };

    assert(!bm_bili_get_received_guards_by_period(&login, "2026-08-01", "2026-08-31", &guards, &err));
    assert(strstr(err.message, "数据查询超时") != NULL);
    int max_delay = 0, total_delay = 0;
    for (int i = 0; i < g_h.delay_count; i++) {
        if (g_h.delays[i] > max_delay) max_delay = g_h.delays[i];
        total_delay += g_h.delays[i];
    }
    assert(max_delay == 10000);
    assert(total_delay < 180000);
    assert(g_h.req_count < 25);

    bm_guard_list_free(&guards);
    printf("PASS bounds polling when results never become ready\n");
}

static void test_gift_stream_cookie_csrf(void) {
    reset_harness(NULL, NULL);
    bm_login_info login;
    memset(&login, 0, sizeof login);
    bm_strlcpy(login.cookies, "SESSDATA=test-session; bili_jct=cookie-csrf; buvid3=test-device", sizeof login.cookies);

    bm_guard_list guards;
    bm_guard_list_init(&guards);
    bm_bili_err err = { 0 };

    assert(bm_bili_get_received_guards_by_period(&login, "2026-08-01", "2026-08-31", &guards, &err));
    assert(g_h.req_count == 1);
    assert(strstr(g_h.reqs[0].body, "csrf_token=cookie-csrf&csrf=cookie-csrf") != NULL);

    bm_guard_list_free(&guards);
    printf("PASS uses CSRF from stored cookies\n");
}

static void test_gift_stream_validation(void) {
    reset_harness(NULL, NULL);
    bm_login_info empty_login;
    memset(&empty_login, 0, sizeof empty_login);
    bm_login_info login = make_test_login();
    bm_guard_list guards;
    bm_guard_list_init(&guards);
    bm_bili_err err = { 0 };

    assert(!bm_bili_get_received_guards_by_period(&empty_login, "2026-08-01", "2026-08-31", &guards, &err));
    assert(strstr(err.message, "登录信息不完整") != NULL);

    assert(!bm_bili_get_received_guards_by_period(&login, NULL, NULL, &guards, &err));
    assert(!bm_bili_get_received_guards_by_period(&login, "2026-08-01", NULL, &guards, &err));
    assert(!bm_bili_get_received_guards_by_period(&login, "2026-02-30", "2026-03-01", &guards, &err));
    assert(!bm_bili_get_received_guards_by_period(&login, "20260801", "20260831", &guards, &err));
    assert(!bm_bili_get_received_guards_by_period(&login, "2026-08-31", "2026-08-01", &guards, &err));
    assert(g_h.req_count == 0);

    bm_guard_list_free(&guards);
    printf("PASS rejects incomplete login and invalid periods before requesting\n");
}

static const char *reply_custom_str(const recorded_req *r, int index, bm_http_resp *resp, void *ctx) {
    (void)r; (void)index;
    const char *s = (const char *)ctx;
    if (strcmp(s, "__TIMEOUT__") == 0) {
        resp->timed_out = true;
        bm_strlcpy(resp->error, "Operation timed out", sizeof resp->error);
        return NULL;
    }
    if (strcmp(s, "__NETERR__") == 0) {
        bm_strlcpy(resp->error, "network failure", sizeof resp->error);
        return NULL;
    }
    return s;
}

static void test_gift_stream_errors(void) {
    bm_login_info login = make_test_login();
    bm_guard_list guards;
    bm_guard_list_init(&guards);
    bm_bili_err err = { 0 };

    reset_harness(reply_custom_str, (void *)"{\"code\":-101,\"message\":\"账号未登录\"}");
    assert(!bm_bili_get_received_guards_by_period(&login, "2026-08-01", "2026-08-31", &guards, &err));
    assert(err.code == -101);
    printf("PASS preserves API login errors\n");

    reset_harness(reply_custom_str, (void *)"<html>blocked</html>");
    assert(!bm_bili_get_received_guards_by_period(&login, "2026-08-01", "2026-08-31", &guards, &err));
    assert(strstr(err.message, "HTML") != NULL);

    reset_harness(reply_custom_str, (void *)"{invalid");
    assert(!bm_bili_get_received_guards_by_period(&login, "2026-08-01", "2026-08-31", &guards, &err));
    assert(strstr(err.message, "JSON") != NULL);

    reset_harness(reply_custom_str, (void *)"__NETERR__");
    assert(!bm_bili_get_received_guards_by_period(&login, "2026-08-01", "2026-08-31", &guards, &err));
    assert(strstr(err.message, "network failure") != NULL);

    reset_harness(reply_custom_str, (void *)"__TIMEOUT__");
    assert(!bm_bili_get_received_guards_by_period(&login, "2026-08-01", "2026-08-31", &guards, &err));
    assert(strstr(err.message, "请求超时") != NULL);
    printf("PASS rejects HTML, malformed JSON, transport failures and timeouts\n");

    const char *bad_payloads[] = {
        "{\"code\":0,\"data\":null}",
        "{\"code\":0,\"data\":{}}",
        "{\"code\":0,\"data\":{\"ready\":1,\"list\":null,\"total_page\":0}}",
        "{\"code\":0,\"data\":{\"ready\":1,\"list\":[],\"total_page\":-1}}",
        "{\"code\":0,\"data\":{\"ready\":1,\"list\":[]}}"
    };
    for (size_t i = 0; i < sizeof bad_payloads / sizeof bad_payloads[0]; i++) {
        reset_harness(reply_custom_str, (void *)bad_payloads[i]);
        assert(!bm_bili_get_received_guards_by_period(&login, "2026-08-01", "2026-08-31", &guards, &err));
        assert(strstr(err.message, "无效") != NULL);
    }
    bm_guard_list_free(&guards);
    printf("PASS rejects incomplete result metadata instead of returning partial records\n");
}

/* --- WBI, SendMessage, Guards, Templates & Store Tests --- */

static const char *reply_send_msg_flow(const recorded_req *r, int index, bm_http_resp *resp, void *ctx) {
    (void)index; (void)ctx;
    if (strstr(r->url, "/x/web-interface/nav") != NULL) {
        bm_strlcpy(resp->set_cookies[resp->set_cookie_count++], "bili_ticket=jwt_ticket_123; Path=/", sizeof resp->set_cookies[0]);
        return "{\"code\":0,\"data\":{\"isLogin\":true,\"wbi_img\":{"
               "\"img_url\":\"https://i0.hdslb.com/bfs/wbi/7cd084941338484aae1ad9425b84077c.png\","
               "\"sub_url\":\"https://i0.hdslb.com/bfs/wbi/4932caff0ff746eab6f01bf08b70ac45.png\"}}}";
    }
    return "{\"code\":0,\"data\":{\"msg_key\":7391827391827391827}}";
}

static void test_wbi_and_send_message(void) {
    reset_harness(reply_send_msg_flow, NULL);
    g_h.now_ms = 1700000000000LL;

    bm_login_info login = make_test_login();
    char msg_key[64] = { 0 };
    bm_bili_err err = { 0 };

    bool ok = bm_bili_send_message(&login, 475210, "你好，舰长！", msg_key, sizeof msg_key, &err);
    assert(ok);
    assert(strcmp(msg_key, "7391827391827391827") == 0);
    assert(g_h.req_count == 2);
    assert(strstr(g_h.reqs[1].url, "/web_im/v1/web_im/send_msg?w_dev_id=TEST-DEV-ID&w_receiver_id=475210&w_sender_uid=123&wts=1700000000&w_rid=") != NULL);
    assert(strstr(g_h.reqs[1].cookie, "bili_ticket=jwt_ticket_123") != NULL);
    printf("PASS WBI signing, bili_ticket capture, and 64-bit msg_key preservation\n");
}

static void test_guards_templates_and_store(void) {
    bm_guard_list list;
    bm_guard_list_init(&list);

    bm_guard g1 = { .uid = 100, .username = "Alice", .guard_level = 3 };
    bm_guard g2 = { .uid = 200, .username = "Bob", .guard_level = 2 };
    bm_guard g3 = { .uid = 100, .username = "Alice", .guard_level = 1 }; /* Higher rank (总督) duplicate */
    bm_guard_list_push(&list, &g1);
    bm_guard_list_push(&list, &g2);
    bm_guard_list_push(&list, &g3);

    int removed = bm_guard_deduplicate(&list);
    assert(removed == 1);
    assert(list.count == 2);
    assert(list.items[0].uid == 100 && list.items[0].guard_level == 1);

    size_t txt_len = 0;
    char *txt = bm_guard_format_txt(&list, &txt_len);
    assert(txt != NULL);

    bm_guard_list imported;
    bm_guard_list_init(&imported);
    assert(bm_guard_import_text(&imported, txt, true));
    assert(imported.count == 2);
    assert(imported.items[0].uid == 100 && imported.items[0].guard_level == 1);
    assert(imported.items[1].uid == 200 && imported.items[1].guard_level == 2);
    free(txt);

    char rendered[512];
    bm_template_process(
        "感谢 {name} ({level}) 在 {date} ({month}月/{lastMonth}月/{day}日) 支持，兑换码：{code}",
        &list.items[0],
        "CODE-888",
        "2026-11-11",
        11,
        10,
        11,
        rendered,
        sizeof rendered
    );
    assert(strcmp(rendered, "感谢 Alice (总督) 在 2026-11-11 (11月/10月/11日) 支持，兑换码：CODE-888") == 0);

    bm_guard_list_free(&list);
    bm_guard_list_free(&imported);
    printf("PASS guard deduplication, TXT import/export, and template variable expansion\n");
}

static void test_electron_store_config_compat(void) {
    /* CTest runs in the build directory, keeping fixtures local and allowing
       Debug and Release tests to run independently. */
    const char *path = "bm_test_config_compat.json";
    /* Match the keys written by Electron v1.5.5: SettingPage, TemplatePage,
       GuardListPage and the codes store. darkMode is a native extension. */
    const char *sample_json =
        "{\n"
        "  \"roomID\": \"21672023\",\n"
        "  \"setting\": {\"sendInterval\": 1500, \"darkMode\": false},\n"
        "  \"loginResponse\": {\n"
        "    \"DedeUserID\": \"475210\",\n"
        "    \"SESSDATA\": \"sess_abc\",\n"
        "    \"bili_jct\": \"csrf_xyz\",\n"
        "    \"buvid3\": \"BUVID-TEST-123\",\n"
        "    \"dev_id\": \"DEVID-TEST-456\",\n"
        "    \"cookies\": \"SESSDATA=sess_abc; bili_jct=csrf_xyz; DedeUserID=475210\"\n"
        "  },\n"
        "  \"guards\": [\n"
        "    {\"uid\": 1001, \"username\": \"GuardA\", \"guard_level\": 3, \"face\": \"https://example.com/a.jpg\"}\n"
        "  ],\n"
        "  \"originalGuards\": [\n"
        "    {\"uid\": 1001, \"username\": \"GuardA\", \"guard_level\": 3, \"face\": \"https://example.com/a.jpg\"}\n"
        "  ],\n"
        "  \"templateList\": [\"欢迎模板\"],\n"
        "  \"template:欢迎模板\": \"你好 {name}\",\n"
        "  \"codes\": [\"CODE-1\", \"CODE-2\"]\n"
        "}\n";

    assert(bm_write_file(path, sample_json, strlen(sample_json)));

    bm_store s;
    bm_store_init(&s);
    assert(bm_store_load(&s, path));
    assert(strcmp(s.room_id, "21672023") == 0);
    assert(s.send_interval == 1500);
    assert(s.dark_mode == false);
    assert(s.login.has_login == true);
    assert(strcmp(s.login.DedeUserID, "475210") == 0);
    assert(strcmp(s.login.SESSDATA, "sess_abc") == 0);
    assert(strcmp(s.login.bili_jct, "csrf_xyz") == 0);
    assert(strcmp(s.login.buvid3, "BUVID-TEST-123") == 0);
    assert(strcmp(s.login.dev_id, "DEVID-TEST-456") == 0);
    assert(strcmp(s.login.cookies, "SESSDATA=sess_abc; bili_jct=csrf_xyz; DedeUserID=475210") == 0);
    assert(s.guards.count == 1 && s.guards.items[0].uid == 1001);
    assert(strcmp(s.guards.items[0].username, "GuardA") == 0);
    assert(s.guards.items[0].guard_level == 3);
    assert(s.original_guards.count == 1 && s.original_guards.items[0].uid == 1001);
    assert(s.template_count == 1 && strcmp(s.templates[0].name, "欢迎模板") == 0);
    assert(strcmp(bm_store_get_template(&s, "欢迎模板"), "你好 {name}") == 0);
    assert(s.code_count == 2 && strcmp(s.codes[0].code, "CODE-1") == 0);

    assert(bm_store_import_codes(&s, "CODE-2\nCODE-3\n\nCODE-4") == 2);
    assert(s.code_count == 4);
    assert(bm_store_save(&s));
    bm_store_free(&s);

    bm_store s2;
    bm_store_init(&s2);
    assert(bm_store_load(&s2, path));
    assert(strcmp(s2.room_id, "21672023") == 0);
    assert(s2.send_interval == 1500 && s2.dark_mode == false);
    assert(s2.login.has_login == true);
    assert(strcmp(s2.login.DedeUserID, "475210") == 0);
    assert(strcmp(s2.login.SESSDATA, "sess_abc") == 0);
    assert(strcmp(s2.login.bili_jct, "csrf_xyz") == 0);
    assert(strcmp(s2.login.buvid3, "BUVID-TEST-123") == 0);
    assert(strcmp(s2.login.dev_id, "DEVID-TEST-456") == 0);
    assert(strcmp(s2.login.cookies, "SESSDATA=sess_abc; bili_jct=csrf_xyz; DedeUserID=475210") == 0);
    assert(s2.guards.count == 1 && s2.guards.items[0].uid == 1001);
    assert(strcmp(s2.guards.items[0].username, "GuardA") == 0);
    assert(s2.guards.items[0].guard_level == 3);
    assert(s2.original_guards.count == 1 && s2.original_guards.items[0].uid == 1001);
    assert(s2.template_count == 1 && strcmp(s2.templates[0].name, "欢迎模板") == 0);
    assert(strcmp(bm_store_get_template(&s2, "欢迎模板"), "你好 {name}") == 0);
    assert(s2.code_count == 4);
    assert(strcmp(s2.codes[0].code, "CODE-1") == 0);
    assert(strcmp(s2.codes[1].code, "CODE-2") == 0);
    assert(strcmp(s2.codes[2].code, "CODE-3") == 0);
    assert(strcmp(s2.codes[3].code, "CODE-4") == 0);
    bm_store_free(&s2);
    assert(remove(path) == 0);
    printf("PASS electron-store config.json load/save compatibility\n");
}

static void test_version_comparison(void) {
    static const struct {
        const char *a;
        const char *b;
        int order;
    } cases[] = {
        {"v2.0.0", "2.0.0", 0},
        {"v1.9.9", "2.0.0", -1},
        {"v2.0.1", "2.0.0", 1},
        {"v2.10.0", "2.9.0", 1},
        {"2.0.10", "2.0.9", 1},
        {"10.0.0", "9.99.99", 1},
        {"2.0.0-rc.1", "2.0.0", -1},
        {"2.0.0-alpha", "2.0.0-alpha.1", -1},
        {"2.0.0-alpha.1", "2.0.0-alpha.beta", -1},
        {"2.0.0-alpha.beta", "2.0.0-beta", -1},
        {"2.0.0-beta", "2.0.0-beta.2", -1},
        {"2.0.0-beta.2", "2.0.0-beta.11", -1},
        {"2.0.0-beta.11", "2.0.0-rc.1", -1},
        {"v2.0.0", "2.0.0-beta.1", 1},
        {"v1.5.5", "2.0.0-beta.1", -1},
        {"2.0.0-rc.1", "2.0.0-rc.1+build", 0},
        {"2.0.0-A", "2.0.0-a", -1},
        {"2.0.0-1", "2.0.0-1a", -1},
        {"2.0.0+build-123", "2.0.0+other.001", 0},
        {"2.0.0-01a", "2.0.0-01b", -1},
        {"99999999999999999999.0.0", "9999999999999999999.0.0", 1},
        {"2.0.0-99999999999999999999", "2.0.0-9999999999999999999", 1},
    };
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        int order = 42;
        assert(bm_version_compare(cases[i].a, cases[i].b, &order));
        assert(order == cases[i].order);
        assert(bm_version_compare(cases[i].b, cases[i].a, &order));
        assert(order == -cases[i].order);
        assert(bm_version_compare(cases[i].a, cases[i].a, &order));
        assert(order == 0);
    }
    static const char *invalid[] = {
        NULL, "", "v", "2", "2.0", "2.0.0.1", "v02.0.0", "2.00.0", "2.0.00",
        "2.0.0-", "2.0.0-01", "2.0.0-alpha.01", "2.0.0-alpha..1",
        "2.0.0-alpha.", "2.0.0+", "2.0.0+build.", "2.0.0+build..1",
        "2.0.0+build+other", "2.0.0-rc_1", " 2.0.0", "2.0.0\n", "release-2.0.0",
    };
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
        int order = 42;
        assert(!bm_version_compare(invalid[i], "2.0.0", &order));
        assert(order == 42);
        assert(!bm_version_compare("2.0.0", invalid[i], &order));
        assert(order == 42);
    }
    assert(!bm_version_compare("2.0.0", "2.0.0", NULL));
    printf("PASS semantic version precedence and invalid version rejection\n");
}

int main(void) {
    test_version_comparison();
    test_gift_stream_levels_and_headers();
    test_gift_stream_pagination();
    test_gift_stream_leap_months();
    test_gift_stream_single_day();
    test_gift_stream_retries();
    test_gift_stream_timeout_bounds();
    test_gift_stream_cookie_csrf();
    test_gift_stream_validation();
    test_gift_stream_errors();
    test_wbi_and_send_message();
    test_guards_templates_and_store();
    test_electron_store_config_compat();
    printf("All test suites passed!\n");
    return 0;
}
