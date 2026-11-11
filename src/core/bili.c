#include "core/bili.h"
#include "core/http.h"
#include "core/md5.h"
#include "core/util.h"
#include "cJSON.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WBI_KEYS_CACHE_DURATION_MS (5 * 60 * 1000)
#define GIFT_STREAM_QUERY_TIMEOUT_MS (180 * 1000)
#define GIFT_STREAM_UA "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/154.0.0.0 Safari/537.36"
#define DEFAULT_BILI_UA "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/147.0.0.0 Safari/537.36"

static char g_wbi_keys_cache[128] = { 0 };
static int64_t g_wbi_keys_cache_time = 0;
static char g_bili_ticket_uid[64] = { 0 };
static char g_bili_ticket_val[512] = { 0 };
static char g_cached_buvid3_uid[64] = { 0 };
static char g_cached_buvid3_val[128] = { 0 };
static char g_cached_devid_uid[64] = { 0 };
static char g_cached_devid_val[128] = { 0 };

void bm_bili_reset_caches(void) {
    g_wbi_keys_cache[0] = '\0';
    g_wbi_keys_cache_time = 0;
    g_bili_ticket_uid[0] = '\0';
    g_bili_ticket_val[0] = '\0';
    g_cached_buvid3_uid[0] = '\0';
    g_cached_buvid3_val[0] = '\0';
    g_cached_devid_uid[0] = '\0';
    g_cached_devid_val[0] = '\0';
}

static void set_err(bm_bili_err *err, int code, const char *msg) {
    if (!err) return;
    err->code = code;
    bm_strlcpy(err->message, msg ? msg : "未知错误", sizeof err->message);
}

void bm_date_list_init(bm_date_list *list) {
    memset(list, 0, sizeof *list);
}

void bm_date_list_free(bm_date_list *list) {
    if (!list) return;
    free(list->dates);
    list->dates = NULL;
    list->count = list->cap = 0;
}

void bm_date_list_clear(bm_date_list *list) {
    if (!list) return;
    list->count = 0;
}

bool bm_date_list_push(bm_date_list *list, const char *date) {
    if (!list || !date || !date[0]) return false;
    if (bm_date_list_contains(list, date)) return true;
    if (list->count + 1 > list->cap) {
        int next_cap = list->cap ? list->cap * 2 : 32;
        char (*next)[16] = (char (*)[16])realloc(list->dates, (size_t)next_cap * sizeof(*list->dates));
        if (!next) return false;
        list->dates = next;
        list->cap = next_cap;
    }
    bm_strlcpy(list->dates[list->count++], date, 16);
    return true;
}

bool bm_date_list_contains(const bm_date_list *list, const char *date) {
    if (!list || !date) return false;
    for (int i = 0; i < list->count; i++) {
        if (strcmp(list->dates[i], date) == 0) return true;
    }
    return false;
}

typedef struct {
    char key[64];
    char val[512];
} cookie_kv;

typedef struct {
    cookie_kv items[32];
    int count;
} cookie_map;

static void cookie_map_set(cookie_map *m, const char *key, const char *val) {
    if (!key || !key[0] || !val || !val[0]) return;
    for (int i = 0; i < m->count; i++) {
        if (strcmp(m->items[i].key, key) == 0) {
            bm_strlcpy(m->items[i].val, val, sizeof m->items[i].val);
            return;
        }
    }
    if (m->count < 32) {
        bm_strlcpy(m->items[m->count].key, key, sizeof m->items[0].key);
        bm_strlcpy(m->items[m->count].val, val, sizeof m->items[0].val);
        m->count++;
    }
}

static const char *cookie_map_get(const cookie_map *m, const char *key) {
    for (int i = 0; i < m->count; i++) {
        if (strcmp(m->items[i].key, key) == 0) return m->items[i].val;
    }
    return NULL;
}

static void cookie_map_parse(cookie_map *m, const char *cookie_str) {
    if (!cookie_str || !cookie_str[0]) return;
    char buf[2048];
    bm_strlcpy(buf, cookie_str, sizeof buf);
    char *p = buf;
    while (p && *p) {
        char *semi = strchr(p, ';');
        if (semi) *semi = '\0';
        char *item = bm_trim(p);
        char *eq = strchr(item, '=');
        if (eq && eq != item) {
            *eq = '\0';
            cookie_map_set(m, bm_trim(item), bm_trim(eq + 1));
        }
        p = semi ? semi + 1 : NULL;
    }
}

static const char *get_buvid3(const bm_login_info *login, const cookie_map *parsed) {
    if (login && login->buvid3[0]) return login->buvid3;
    const char *from_cookie = cookie_map_get(parsed, "buvid3");
    if (from_cookie && from_cookie[0]) return from_cookie;

    const char *uid = (login && login->DedeUserID[0]) ? login->DedeUserID : "default";
    if (strcmp(g_cached_buvid3_uid, uid) != 0 || !g_cached_buvid3_val[0]) {
        bm_strlcpy(g_cached_buvid3_uid, uid, sizeof g_cached_buvid3_uid);
        bm_uuid_v4(g_cached_buvid3_val, sizeof g_cached_buvid3_val, true);
    }
    return g_cached_buvid3_val;
}

static const char *get_message_dev_id(const bm_login_info *login) {
    if (login && login->dev_id[0]) return login->dev_id;
    const char *uid = (login && login->DedeUserID[0]) ? login->DedeUserID : "";
    if (!uid[0]) {
        bm_uuid_v4(g_cached_devid_val, sizeof g_cached_devid_val, true);
        return g_cached_devid_val;
    }
    if (strcmp(g_cached_devid_uid, uid) != 0 || !g_cached_devid_val[0]) {
        bm_strlcpy(g_cached_devid_uid, uid, sizeof g_cached_devid_uid);
        bm_uuid_v4(g_cached_devid_val, sizeof g_cached_devid_val, true);
    }
    return g_cached_devid_val;
}

void bm_bili_cookie_string(const bm_login_info *login, char *out, size_t cap) {
    if (!cap) return;
    out[0] = '\0';
    if (!login) return;

    cookie_map m;
    memset(&m, 0, sizeof m);
    if (login->cookies[0]) {
        cookie_map_parse(&m, login->cookies);
    }
    if (login->SESSDATA[0]) {
        char norm[512];
        bm_normalize_sessdata(login->SESSDATA, norm, sizeof norm);
        cookie_map_set(&m, "SESSDATA", norm);
    }
    if (login->DedeUserID[0]) {
        cookie_map_set(&m, "DedeUserID", login->DedeUserID);
    }
    if (login->DedeUserID__ckMd5[0]) {
        cookie_map_set(&m, "DedeUserID__ckMd5", login->DedeUserID__ckMd5);
    }
    if (login->bili_jct[0]) {
        cookie_map_set(&m, "bili_jct", login->bili_jct);
    }
    if (login->buvid4[0] && !cookie_map_get(&m, "buvid4")) cookie_map_set(&m, "buvid4", login->buvid4);
    if (login->_uuid[0] && !cookie_map_get(&m, "_uuid")) cookie_map_set(&m, "_uuid", login->_uuid);
    if (login->b_nut[0] && !cookie_map_get(&m, "b_nut")) cookie_map_set(&m, "b_nut", login->b_nut);
    if (login->b_lsid[0] && !cookie_map_get(&m, "b_lsid")) cookie_map_set(&m, "b_lsid", login->b_lsid);
    if (login->sid[0] && !cookie_map_get(&m, "sid")) cookie_map_set(&m, "sid", login->sid);

    const char *ticket = login->bili_ticket[0] ? login->bili_ticket : NULL;
    if (!ticket && login->DedeUserID[0] && strcmp(g_bili_ticket_uid, login->DedeUserID) == 0 && g_bili_ticket_val[0]) {
        ticket = g_bili_ticket_val;
    }
    if (ticket && !cookie_map_get(&m, "bili_ticket")) {
        cookie_map_set(&m, "bili_ticket", ticket);
    }
    if (!cookie_map_get(&m, "buvid3")) {
        cookie_map_set(&m, "buvid3", get_buvid3(login, &m));
    }

    size_t pos = 0;
    for (int i = 0; i < m.count; i++) {
        int n = snprintf(out + pos, cap - pos, "%s%s=%s", i > 0 ? "; " : "", m.items[i].key, m.items[i].val);
        if (n > 0 && pos + (size_t)n < cap) pos += (size_t)n;
    }
}

void bm_bili_extract_csrf(const bm_login_info *login, char *out, size_t cap) {
    if (!cap) return;
    out[0] = '\0';
    if (!login) return;
    if (login->bili_jct[0]) {
        bm_strlcpy(out, login->bili_jct, cap);
        return;
    }
    cookie_map m;
    memset(&m, 0, sizeof m);
    cookie_map_parse(&m, login->cookies);
    const char *csrf = cookie_map_get(&m, "bili_jct");
    if (csrf) bm_strlcpy(out, csrf, cap);
}

static int cmp_kv(const void *a, const void *b) {
    const bm_kv_pair *x = (const bm_kv_pair *)a;
    const bm_kv_pair *y = (const bm_kv_pair *)b;
    return strcmp(x->key, y->key);
}

void bm_bili_wbi_sign(
    const bm_kv_pair *params,
    int count,
    const char *wbi_key,
    int64_t wts,
    char *out_query,
    size_t cap
) {
    static const int mixin_tab[64] = {
        46, 47, 18, 2, 53, 8, 23, 32, 15, 50, 10, 31, 58, 3, 45, 35, 27, 43, 5, 49, 33, 9, 42,
        19, 29, 28, 14, 39, 12, 38, 41, 13, 37, 48, 7, 16, 24, 55, 40, 61, 26, 17, 0, 1, 60,
        51, 30, 4, 22, 25, 54, 21, 56, 59, 6, 63, 57, 62, 11, 36, 20, 34, 44, 52
    };
    char mixin_key[33] = { 0 };
    size_t klen = wbi_key ? strlen(wbi_key) : 0;
    int mpos = 0;
    for (int i = 0; i < 64 && mpos < 32; i++) {
        if ((size_t)mixin_tab[i] < klen) {
            mixin_key[mpos++] = wbi_key[mixin_tab[i]];
        }
    }
    mixin_key[mpos] = '\0';

    char wts_str[32];
    snprintf(wts_str, sizeof wts_str, "%lld", (long long)wts);

    bm_kv_pair sorted[32];
    int total = 0;
    for (int i = 0; i < count && total < 31; i++) {
        sorted[total++] = params[i];
    }
    sorted[total].key = "wts";
    sorted[total].value = wts_str;
    total++;

    qsort(sorted, (size_t)total, sizeof(bm_kv_pair), cmp_kv);

    char query[2048] = { 0 };
    size_t qpos = 0;
    for (int i = 0; i < total; i++) {
        char enc_val[512];
        bm_url_encode_wbi(sorted[i].value, enc_val, sizeof enc_val);
        int n = snprintf(query + qpos, sizeof query - qpos, "%s%s=%s", i > 0 ? "&" : "", sorted[i].key, enc_val);
        if (n > 0 && qpos + (size_t)n < sizeof query) qpos += (size_t)n;
    }

    char to_hash[2112];
    snprintf(to_hash, sizeof to_hash, "%s%s", query, mixin_key);
    char w_rid[33];
    bm_md5_hex(to_hash, strlen(to_hash), w_rid);

    snprintf(out_query, cap, "%s&w_rid=%s", query, w_rid);
}

static bool extract_wbi_stem(const char *url, char *out, size_t cap) {
    if (!url) return false;
    const char *wbi = strstr(url, "wbi/");
    if (!wbi) return false;
    wbi += 4;
    const char *dot = strchr(wbi, '.');
    if (!dot || dot <= wbi) return false;
    size_t len = (size_t)(dot - wbi);
    if (len >= cap) len = cap - 1;
    memcpy(out, wbi, len);
    out[len] = '\0';
    return true;
}

bool bm_bili_get_wbi_keys(const bm_login_info *login, char *out_key, size_t cap, bm_bili_err *err) {
    int64_t now = bm_now_ms();
    if (g_wbi_keys_cache[0] && (now - g_wbi_keys_cache_time) < WBI_KEYS_CACHE_DURATION_MS) {
        bm_strlcpy(out_key, g_wbi_keys_cache, cap);
        return true;
    }

    char cookie[2048];
    bm_bili_cookie_string(login, cookie, sizeof cookie);

    bm_http_resp resp;
    bm_http_req req = {
        .url = "https://api.bilibili.com/x/web-interface/nav",
        .method = "GET",
        .cookie = cookie,
        .user_agent = DEFAULT_BILI_UA
    };
    if (!bm_http_request(&req, &resp)) {
        set_err(err, -1, resp.error);
        bm_http_resp_free(&resp);
        return false;
    }

    for (int i = 0; i < resp.set_cookie_count; i++) {
        const char *hit = strstr(resp.set_cookies[i], "bili_ticket=");
        if (hit) {
            hit += strlen("bili_ticket=");
            const char *semi = strchr(hit, ';');
            size_t tlen = semi ? (size_t)(semi - hit) : strlen(hit);
            if (tlen > 0 && tlen < sizeof g_bili_ticket_val) {
                memcpy(g_bili_ticket_val, hit, tlen);
                g_bili_ticket_val[tlen] = '\0';
                bm_strlcpy(g_bili_ticket_uid, (login && login->DedeUserID[0]) ? login->DedeUserID : "", sizeof g_bili_ticket_uid);
            }
        }
    }

    cJSON *root = cJSON_Parse(resp.data ? resp.data : "");
    bm_http_resp_free(&resp);
    if (!root) {
        set_err(err, -1, "解析 nav 接口响应失败");
        return false;
    }

    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    const cJSON *wbi_img = data ? cJSON_GetObjectItemCaseSensitive(data, "wbi_img") : NULL;
    const cJSON *img_url = wbi_img ? cJSON_GetObjectItemCaseSensitive(wbi_img, "img_url") : NULL;
    const cJSON *sub_url = wbi_img ? cJSON_GetObjectItemCaseSensitive(wbi_img, "sub_url") : NULL;

    char img_key[64] = { 0 }, sub_key[64] = { 0 };
    if (cJSON_IsString(img_url) && cJSON_IsString(sub_url) &&
        extract_wbi_stem(img_url->valuestring, img_key, sizeof img_key) &&
        extract_wbi_stem(sub_url->valuestring, sub_key, sizeof sub_key)) {
        snprintf(g_wbi_keys_cache, sizeof g_wbi_keys_cache, "%s%s", img_key, sub_key);
        g_wbi_keys_cache_time = bm_now_ms();
        bm_strlcpy(out_key, g_wbi_keys_cache, cap);
        cJSON_Delete(root);
        return true;
    }

    const cJSON *msg = cJSON_GetObjectItemCaseSensitive(root, "message");
    const cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");
    set_err(err, cJSON_IsNumber(code) ? code->valueint : -1, cJSON_IsString(msg) ? msg->valuestring : "获取 WBI 密钥失败");
    cJSON_Delete(root);
    return false;
}

bool bm_bili_check_cookies(const bm_login_info *login, bool *out_is_login, bm_bili_err *err) {
    if (out_is_login) *out_is_login = false;
    if (!login || !login->has_login) return false;

    char cookie[2048];
    bm_bili_cookie_string(login, cookie, sizeof cookie);

    bm_http_resp resp;
    bm_http_req req = {
        .url = "https://api.bilibili.com/x/web-interface/nav",
        .method = "GET",
        .cookie = cookie
    };
    if (!bm_http_request(&req, &resp)) {
        set_err(err, -1, resp.error);
        bm_http_resp_free(&resp);
        return false;
    }

    cJSON *root = cJSON_Parse(resp.data ? resp.data : "");
    bm_http_resp_free(&resp);
    if (!root) {
        set_err(err, -1, "解析登录状态失败");
        return false;
    }

    const cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    const cJSON *is_login = data ? cJSON_GetObjectItemCaseSensitive(data, "isLogin") : NULL;
    if (out_is_login) {
        *out_is_login = cJSON_IsTrue(is_login);
    }
    bool ok = cJSON_IsNumber(code) && code->valueint == 0;
    cJSON_Delete(root);
    return ok;
}

static void normalize_face_url(const char *raw, char *out, size_t cap) {
    if (!raw || !raw[0]) {
        bm_strlcpy(out, "static/noface.jpg", cap);
        return;
    }
    if (strncmp(raw, "//", 2) == 0) {
        snprintf(out, cap, "https:%s", raw);
    } else if (strncmp(raw, "http://", 7) == 0) {
        snprintf(out, cap, "https://%s", raw + 7);
    } else {
        bm_strlcpy(out, raw, cap);
    }
}

bool bm_bili_get_user_info(const bm_login_info *login, uint64_t mid, bm_user_profile *out, bm_bili_err *err) {
    memset(out, 0, sizeof *out);
    char url[256];
    snprintf(url, sizeof url, "https://line3-h5-mobile-api.biligame.com/game/center/h5/user/space/info?uid=%llu&sdk_type=1", (unsigned long long)mid);

    char cookie[2048];
    bm_bili_cookie_string(login, cookie, sizeof cookie);

    bm_http_resp resp;
    bm_http_req req = {
        .url = url,
        .method = "GET",
        .cookie = cookie
    };
    if (!bm_http_request(&req, &resp)) {
        set_err(err, -1, resp.error);
        bm_http_resp_free(&resp);
        return false;
    }

    cJSON *root = cJSON_Parse(resp.data ? resp.data : "");
    bm_http_resp_free(&resp);
    if (!root) {
        set_err(err, -1, "解析用户信息失败");
        return false;
    }

    const cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsNumber(code) || code->valueint != 0 || !cJSON_IsObject(data)) {
        const cJSON *msg = cJSON_GetObjectItemCaseSensitive(root, "message");
        set_err(err, cJSON_IsNumber(code) ? code->valueint : -1, cJSON_IsString(msg) ? msg->valuestring : "获取用户信息失败");
        cJSON_Delete(root);
        return false;
    }

    const cJSON *uname = cJSON_GetObjectItemCaseSensitive(data, "uname");
    const cJSON *face = cJSON_GetObjectItemCaseSensitive(data, "face");
    const cJSON *mid_item = cJSON_GetObjectItemCaseSensitive(data, "mid");
    out->mid = cJSON_IsNumber(mid_item) ? (uint64_t)mid_item->valuedouble : mid;
    if (cJSON_IsString(uname) && uname->valuestring) bm_strlcpy(out->uname, uname->valuestring, sizeof out->uname);
    normalize_face_url(cJSON_IsString(face) ? face->valuestring : "", out->face, sizeof out->face);
    cJSON_Delete(root);
    return true;
}

bool bm_bili_search_user(const bm_login_info *login, const char *username, bm_user_profile *out, bm_bili_err *err) {
    memset(out, 0, sizeof *out);
    if (!username || !username[0]) {
        set_err(err, -1, "请输入用户名");
        return false;
    }

    char enc_kw[256];
    bm_url_encode(username, enc_kw, sizeof enc_kw);
    char url[512];
    snprintf(url, sizeof url, "https://api.bilibili.com/x/web-interface/search/type?keyword=%s&page=1&search_type=bili_user&order=totalrank&pagesize=5", enc_kw);

    char cookie[2048];
    bm_bili_cookie_string(login, cookie, sizeof cookie);

    bm_http_resp resp;
    bm_http_req req = {
        .url = url,
        .method = "GET",
        .cookie = cookie
    };
    if (!bm_http_request(&req, &resp)) {
        set_err(err, -1, resp.error);
        bm_http_resp_free(&resp);
        return false;
    }

    cJSON *root = cJSON_Parse(resp.data ? resp.data : "");
    bm_http_resp_free(&resp);
    if (!root) {
        set_err(err, -1, "解析搜索结果失败");
        return false;
    }

    const cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    const cJSON *result = data ? cJSON_GetObjectItemCaseSensitive(data, "result") : NULL;
    if (!cJSON_IsNumber(code) || code->valueint != 0 || !cJSON_IsArray(result) || cJSON_GetArraySize(result) == 0) {
        set_err(err, cJSON_IsNumber(code) ? code->valueint : -1, "未找到匹配的用户");
        cJSON_Delete(root);
        return false;
    }

    const cJSON *first = cJSON_GetArrayItem(result, 0);
    const cJSON *uname = cJSON_GetObjectItemCaseSensitive(first, "uname");
    const cJSON *upic = cJSON_GetObjectItemCaseSensitive(first, "upic");
    const cJSON *mid = cJSON_GetObjectItemCaseSensitive(first, "mid");
    if (!cJSON_IsString(uname) || !uname->valuestring || strcmp(uname->valuestring, username) != 0) {
        set_err(err, -1, "no matched result");
        cJSON_Delete(root);
        return false;
    }

    out->mid = cJSON_IsNumber(mid) ? (uint64_t)mid->valuedouble : 0;
    bm_strlcpy(out->uname, uname->valuestring, sizeof out->uname);
    normalize_face_url(cJSON_IsString(upic) ? upic->valuestring : "", out->face, sizeof out->face);
    cJSON_Delete(root);
    return true;
}

bool bm_bili_get_room_info(const char *room_id, bm_room_info *out, bm_bili_err *err) {
    memset(out, 0, sizeof *out);
    if (!room_id || !room_id[0]) {
        set_err(err, -1, "Invalid Room ID");
        return false;
    }

    char url[256];
    snprintf(url, sizeof url, "https://api.live.bilibili.com/xlive/web-room/v1/index/getH5InfoByRoom?room_id=%s", room_id);

    bm_http_resp resp;
    bm_http_req req = { .url = url, .method = "GET" };
    if (!bm_http_request(&req, &resp)) {
        set_err(err, -1, resp.error);
        bm_http_resp_free(&resp);
        return false;
    }

    cJSON *root = cJSON_Parse(resp.data ? resp.data : "");
    bm_http_resp_free(&resp);
    if (!root) {
        set_err(err, -1, "解析直播间信息失败");
        return false;
    }

    const cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    const cJSON *rinfo = data ? cJSON_GetObjectItemCaseSensitive(data, "room_info") : NULL;
    const cJSON *ainfo = data ? cJSON_GetObjectItemCaseSensitive(data, "anchor_info") : NULL;
    if (!cJSON_IsNumber(code) || code->valueint != 0 || !cJSON_IsObject(rinfo)) {
        set_err(err, -1, "不存在的直播间，请检查是否填写正确");
        cJSON_Delete(root);
        return false;
    }

    const cJSON *rid = cJSON_GetObjectItemCaseSensitive(rinfo, "room_id");
    const cJSON *uid = cJSON_GetObjectItemCaseSensitive(rinfo, "uid");
    const cJSON *title = cJSON_GetObjectItemCaseSensitive(rinfo, "title");
    const cJSON *cover = cJSON_GetObjectItemCaseSensitive(rinfo, "cover");
    const cJSON *online = cJSON_GetObjectItemCaseSensitive(rinfo, "online");
    const cJSON *live_status = cJSON_GetObjectItemCaseSensitive(rinfo, "live_status");
    const cJSON *live_start = cJSON_GetObjectItemCaseSensitive(rinfo, "live_start_time");

    out->room_id = cJSON_IsNumber(rid) ? (uint64_t)rid->valuedouble : (uint64_t)strtoull(room_id, NULL, 10);
    out->uid = cJSON_IsNumber(uid) ? (uint64_t)uid->valuedouble : 0;
    if (cJSON_IsString(title) && title->valuestring) bm_strlcpy(out->title, title->valuestring, sizeof out->title);
    normalize_face_url(cJSON_IsString(cover) ? cover->valuestring : "", out->cover, sizeof out->cover);
    out->online = cJSON_IsNumber(online) ? online->valueint : 0;
    out->live_status = cJSON_IsNumber(live_status) ? live_status->valueint : 0;
    out->live_start_time = cJSON_IsNumber(live_start) ? (int64_t)live_start->valuedouble : 0;

    if (cJSON_IsObject(ainfo)) {
        const cJSON *base = cJSON_GetObjectItemCaseSensitive(ainfo, "base_info");
        const cJSON *rel = cJSON_GetObjectItemCaseSensitive(ainfo, "relation_info");
        if (cJSON_IsObject(base)) {
            const cJSON *uname = cJSON_GetObjectItemCaseSensitive(base, "uname");
            const cJSON *face = cJSON_GetObjectItemCaseSensitive(base, "face");
            if (cJSON_IsString(uname) && uname->valuestring) bm_strlcpy(out->anchor_uname, uname->valuestring, sizeof out->anchor_uname);
            normalize_face_url(cJSON_IsString(face) ? face->valuestring : "", out->anchor_face, sizeof out->anchor_face);
        }
        if (cJSON_IsObject(rel)) {
            const cJSON *att = cJSON_GetObjectItemCaseSensitive(rel, "attention");
            out->attention = cJSON_IsNumber(att) ? att->valueint : 0;
        }
    }

    cJSON_Delete(root);
    return true;
}

bool bm_bili_get_vtbs_history(uint64_t uid, bool is_guard, bm_chart_history *out, bm_bili_err *err) {
    memset(out, 0, sizeof *out);
    if (uid == 0) return false;

    char url[256];
    snprintf(url, sizeof url, "https://api.vtbs.moe/v2/%s/%llu", is_guard ? "bulkGuard" : "bulkActiveSome", (unsigned long long)uid);

    bm_http_resp resp;
    bm_http_req req = { .url = url, .method = "GET", .timeout_ms = 15000 };
    if (!bm_http_request(&req, &resp)) {
        set_err(err, -1, resp.error);
        bm_http_resp_free(&resp);
        return false;
    }

    cJSON *arr = cJSON_Parse(resp.data ? resp.data : "");
    bm_http_resp_free(&resp);
    if (!cJSON_IsArray(arr)) {
        if (arr) cJSON_Delete(arr);
        set_err(err, -1, "解析 vtbs.moe 历史数据失败");
        return false;
    }

    int n = cJSON_GetArraySize(arr);
    if (n <= 0) {
        cJSON_Delete(arr);
        return true;
    }

    /* Sample backwards from newest up to 61 days, matching SummaryPage.vue */
    char rev_labels[BM_MAX_HISTORY_POINTS][16];
    float rev_values[BM_MAX_HISTORY_POINTS];
    int count = 0;

    const cJSON *newest = cJSON_GetArrayItem(arr, n - 1);
    const cJSON *newest_t = cJSON_GetObjectItemCaseSensitive(newest, "time");
    if (!cJSON_IsNumber(newest_t)) {
        cJSON_Delete(arr);
        return false;
    }

    char target_ymd[16];
    bm_unix_to_ymd((int64_t)(newest_t->valuedouble / 1000.0), target_ymd, sizeof target_ymd);
    int ty = 0, tm = 0, td = 0;
    bm_parse_ymd(target_ymd, &ty, &tm, &td);
    int64_t target_days = bm_ymd_to_days(ty, tm, td);

    int day_count = 60;
    for (int i = n - 1; i >= 0 && day_count >= 0 && count < BM_MAX_HISTORY_POINTS; i--) {
        const cJSON *item = cJSON_GetArrayItem(arr, i);
        const cJSON *t = cJSON_GetObjectItemCaseSensitive(item, "time");
        const cJSON *v = cJSON_GetObjectItemCaseSensitive(item, is_guard ? "guardNum" : "follower");
        if (!cJSON_IsNumber(t) || !cJSON_IsNumber(v)) continue;

        int64_t sec = (int64_t)(t->valuedouble / 1000.0);
        char cur_ymd[16];
        bm_unix_to_ymd(sec, cur_ymd, sizeof cur_ymd);
        int cy = 0, cm = 0, cd = 0;
        if (!bm_parse_ymd(cur_ymd, &cy, &cm, &cd)) continue;
        int64_t cur_days = bm_ymd_to_days(cy, cm, cd);

        if (cur_days <= target_days) {
            bm_unix_to_mmdd(sec, rev_labels[count], sizeof rev_labels[0]);
            rev_values[count] = (float)v->valuedouble;
            count++;
            if (cur_days < target_days) {
                target_days = cur_days;
            } else {
                target_days -= 1;
            }
            day_count -= 1;
        }
    }

    for (int i = 0; i < count; i++) {
        bm_strlcpy(out->labels[i], rev_labels[count - 1 - i], sizeof out->labels[0]);
        out->values[i] = rev_values[count - 1 - i];
    }
    out->count = count;
    cJSON_Delete(arr);
    return true;
}

static void append_guard_json_array(const cJSON *arr, bm_guard_list *out) {
    if (!cJSON_IsArray(arr)) return;
    const cJSON *el = NULL;
    cJSON_ArrayForEach(el, arr) {
        if (!cJSON_IsObject(el)) continue;
        bm_guard g;
        memset(&g, 0, sizeof g);
        const cJSON *face = cJSON_GetObjectItemCaseSensitive(el, "face");
        const cJSON *uname = cJSON_GetObjectItemCaseSensitive(el, "username");
        const cJSON *glevel = cJSON_GetObjectItemCaseSensitive(el, "guard_level");
        const cJSON *alive = cJSON_GetObjectItemCaseSensitive(el, "is_alive");
        const cJSON *rank = cJSON_GetObjectItemCaseSensitive(el, "rank");
        const cJSON *ruid = cJSON_GetObjectItemCaseSensitive(el, "ruid");
        const cJSON *uid = cJSON_GetObjectItemCaseSensitive(el, "uid");

        normalize_face_url(cJSON_IsString(face) ? face->valuestring : "", g.face, sizeof g.face);
        if (cJSON_IsString(uname) && uname->valuestring) bm_strlcpy(g.username, uname->valuestring, sizeof g.username);
        g.guard_level = cJSON_IsNumber(glevel) ? glevel->valueint : 3;
        g.is_alive = cJSON_IsNumber(alive) ? alive->valueint : 0;
        g.rank = cJSON_IsNumber(rank) ? rank->valueint : 1;
        g.ruid = cJSON_IsNumber(ruid) ? (uint64_t)ruid->valuedouble : 61639371ULL;
        g.uid = cJSON_IsNumber(uid) ? (uint64_t)uid->valuedouble : 0;
        if (g.uid != 0) bm_guard_list_push(out, &g);
    }
}

static bool fetch_guard_page(uint64_t ruid, int page, int page_size, bm_guard_list *out, int *out_total_pages, bm_bili_err *err) {
    char url[256];
    snprintf(url, sizeof url, "https://api.live.bilibili.com/xlive/app-room/v2/guardTab/topList?roomid=1&page=%d&page_size=%d&ruid=%llu",
             page, page_size, (unsigned long long)ruid);
    bm_http_resp resp;
    bm_http_req req = { .url = url, .method = "GET" };
    if (!bm_http_request(&req, &resp)) {
        set_err(err, -1, resp.error);
        bm_http_resp_free(&resp);
        return false;
    }

    cJSON *root = cJSON_Parse(resp.data ? resp.data : "");
    bm_http_resp_free(&resp);
    if (!root) {
        set_err(err, -1, "解析舰长列表失败");
        return false;
    }

    const cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    if (!cJSON_IsNumber(code) || code->valueint != 0 || !cJSON_IsObject(data)) {
        set_err(err, cJSON_IsNumber(code) ? code->valueint : -1, "获取舰长列表失败");
        cJSON_Delete(root);
        return false;
    }

    if (page == 1) {
        append_guard_json_array(cJSON_GetObjectItemCaseSensitive(data, "top3"), out);
        const cJSON *info = cJSON_GetObjectItemCaseSensitive(data, "info");
        const cJSON *pg = info ? cJSON_GetObjectItemCaseSensitive(info, "page") : NULL;
        if (out_total_pages) *out_total_pages = cJSON_IsNumber(pg) ? pg->valueint : 1;
    }
    append_guard_json_array(cJSON_GetObjectItemCaseSensitive(data, "list"), out);
    cJSON_Delete(root);
    return true;
}

bool bm_bili_get_guard_list(uint64_t ruid, bm_guard_list *out, bm_bili_err *err) {
    bm_guard_list_clear(out);
    int total_pages = 1;
    if (!fetch_guard_page(ruid, 1, 30, out, &total_pages, err)) {
        return false;
    }
    for (int p = 2; p <= total_pages; p++) {
        if (!fetch_guard_page(ruid, p, 30, out, NULL, err)) {
            return false;
        }
    }
    return true;
}

bool bm_bili_get_guard_valid_dates(const char *room_id, bm_date_list *out, bm_bili_err *err) {
    bm_date_list_clear(out);
    char today[16];
    bm_date_today_str(today, sizeof today);
    bm_date_list_push(out, today);

    if (!room_id || !room_id[0]) return false;
    char url[256];
    snprintf(url, sizeof url, "http://guard.vjoi.cn/day?room=%s", room_id);

    bm_http_resp resp;
    bm_http_req req = { .url = url, .method = "GET", .timeout_ms = 10000 };
    if (!bm_http_request(&req, &resp)) {
        set_err(err, -1, resp.error);
        bm_http_resp_free(&resp);
        return false;
    }

    cJSON *root = cJSON_Parse(resp.data ? resp.data : "");
    bm_http_resp_free(&resp);
    if (!root) return false;

    const cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "Code");
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "Data");
    if (cJSON_IsNumber(code) && code->valueint == 0 && cJSON_IsArray(data)) {
        const cJSON *d = NULL;
        cJSON_ArrayForEach(d, data) {
            if (cJSON_IsString(d) && d->valuestring) {
                bm_date_list_push(out, d->valuestring);
            }
        }
        cJSON_Delete(root);
        return true;
    }
    cJSON_Delete(root);
    return false;
}

bool bm_bili_get_guard_snapshot(const char *room_id, const char *date, bm_guard_list *out, bm_bili_err *err) {
    bm_guard_list_clear(out);
    char today[16];
    bm_date_today_str(today, sizeof today);
    if (!date || !date[0] || strcmp(date, today) == 0) {
        bm_room_info rinfo;
        if (!bm_bili_get_room_info(room_id, &rinfo, err)) return false;
        return bm_bili_get_guard_list(rinfo.uid, out, err);
    }

    char url[256];
    snprintf(url, sizeof url, "http://guard.vjoi.cn/history?room=%s&date=%s", room_id, date);
    bm_http_resp resp;
    bm_http_req req = { .url = url, .method = "GET", .timeout_ms = 15000 };
    if (!bm_http_request(&req, &resp)) {
        set_err(err, -1, resp.error);
        bm_http_resp_free(&resp);
        return false;
    }

    cJSON *root = cJSON_Parse(resp.data ? resp.data : "");
    bm_http_resp_free(&resp);
    if (!root) {
        set_err(err, -1, "解析历史快照失败");
        return false;
    }

    const cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "Code");
    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "Data");
    if (!cJSON_IsNumber(code) || code->valueint != 0 || !cJSON_IsArray(data)) {
        set_err(err, -1, "该日期没有可用的快照数据");
        cJSON_Delete(root);
        return false;
    }

    const cJSON *el = NULL;
    cJSON_ArrayForEach(el, data) {
        bm_guard g;
        memset(&g, 0, sizeof g);
        const cJSON *face = cJSON_GetObjectItemCaseSensitive(el, "Face");
        const cJSON *uname = cJSON_GetObjectItemCaseSensitive(el, "Username");
        const cJSON *glevel = cJSON_GetObjectItemCaseSensitive(el, "Guard_level");
        const cJSON *uid = cJSON_GetObjectItemCaseSensitive(el, "Uid");

        normalize_face_url(cJSON_IsString(face) ? face->valuestring : "", g.face, sizeof g.face);
        if (cJSON_IsString(uname) && uname->valuestring) bm_strlcpy(g.username, uname->valuestring, sizeof g.username);
        if (cJSON_IsNumber(glevel)) g.guard_level = glevel->valueint;
        else if (cJSON_IsString(glevel) && glevel->valuestring) g.guard_level = atoi(glevel->valuestring);
        else g.guard_level = 3;
        g.is_alive = 0;
        g.rank = 1;
        g.ruid = 61639371ULL;
        if (cJSON_IsNumber(uid)) g.uid = (uint64_t)uid->valuedouble;
        else if (cJSON_IsString(uid) && uid->valuestring) g.uid = (uint64_t)strtoull(uid->valuestring, NULL, 10);
        if (g.uid != 0) bm_guard_list_push(out, &g);
    }
    cJSON_Delete(root);
    return true;
}

static void extract_raw_msg_key(const char *json_str, char *out, size_t cap) {
    if (!cap) return;
    out[0] = '\0';
    if (!json_str) return;
    const char *hit = strstr(json_str, "\"msg_key\"");
    if (!hit) return;
    const char *colon = strchr(hit + 9, ':');
    if (!colon) return;
    const char *p = colon + 1;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '"') {
        p++;
        size_t i = 0;
        while (*p && *p != '"' && i + 1 < cap) out[i++] = *p++;
        out[i] = '\0';
    } else {
        size_t i = 0;
        while (*p && isdigit((unsigned char)*p) && i + 1 < cap) out[i++] = *p++;
        out[i] = '\0';
    }
}

static bool handle_im_message(
    const bm_login_info *login,
    const char *post_data,
    const char *signed_query,
    char *out_msg_key,
    size_t key_cap,
    bm_bili_err *err
) {
    char url[1024];
    if (signed_query && signed_query[0]) {
        snprintf(url, sizeof url, "https://api.vc.bilibili.com/web_im/v1/web_im/send_msg?%s", signed_query);
    } else {
        bm_strlcpy(url, "https://api.vc.bilibili.com/web_im/v1/web_im/send_msg", sizeof url);
    }

    char cookie[2048];
    bm_bili_cookie_string(login, cookie, sizeof cookie);

    static const char *extra_hdrs[] = {
        "Accept-Language: zh-CN,zh;q=0.9,en;q=0.8",
        "Cache-Control: no-cache",
        "Pragma: no-cache",
        "Priority: u=1, i",
        "sec-ch-ua: \"Google Chrome\";v=\"147\", \"Not.A/Brand\";v=\"8\", \"Chromium\";v=\"147\"",
        "sec-ch-ua-mobile: ?0",
        "sec-ch-ua-platform: \"macOS\"",
        "sec-fetch-dest: empty",
        "sec-fetch-mode: cors",
        "sec-fetch-site: same-site"
    };

    bm_http_resp resp;
    bm_http_req req = {
        .url = url,
        .method = "POST",
        .body = post_data,
        .cookie = cookie,
        .accept = "*/*",
        .content_type = "application/x-www-form-urlencoded",
        .origin = "https://message.bilibili.com",
        .referer = "https://message.bilibili.com/",
        .user_agent = DEFAULT_BILI_UA,
        .extra_headers = extra_hdrs,
        .extra_header_count = (int)(sizeof extra_hdrs / sizeof extra_hdrs[0])
    };
    if (!bm_http_request(&req, &resp)) {
        set_err(err, -1, resp.error);
        bm_http_resp_free(&resp);
        return false;
    }

    const char *raw = resp.data ? resp.data : "";
    while (*raw && isspace((unsigned char)*raw)) raw++;
    if (*raw == '<') {
        bm_http_resp_free(&resp);
        set_err(err, -1, "API返回了HTML响应，可能是登录失效或请求被拒绝");
        return false;
    }

    if (out_msg_key && key_cap > 0) {
        extract_raw_msg_key(raw, out_msg_key, key_cap);
    }

    cJSON *root = cJSON_Parse(raw);
    bm_http_resp_free(&resp);
    if (!root) {
        set_err(err, -1, "解析私信接口响应失败");
        return false;
    }

    const cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");
    const cJSON *msg = cJSON_GetObjectItemCaseSensitive(root, "message");
    if (!cJSON_IsNumber(code) || code->valueint != 0) {
        int c = cJSON_IsNumber(code) ? code->valueint : -1;
        char err_buf[512];
        if (cJSON_IsString(msg) && msg->valuestring && msg->valuestring[0]) {
            snprintf(err_buf, sizeof err_buf, "(%d) %s", c, msg->valuestring);
        } else {
            snprintf(err_buf, sizeof err_buf, "错误码: %d", c);
        }
        set_err(err, c, err_buf);
        cJSON_Delete(root);
        return false;
    }

    cJSON_Delete(root);
    return true;
}

bool bm_bili_send_message(
    const bm_login_info *login,
    uint64_t target_uid,
    const char *content,
    char *out_msg_key,
    size_t key_cap,
    bm_bili_err *err
) {
    if (!bm_login_is_complete(login)) {
        set_err(err, -1, "登录信息不完整，请退出账号后重新扫码登录");
        return false;
    }
    if (target_uid == 0) {
        set_err(err, -1, "私信目标 UID 无效");
        return false;
    }

    char wbi_key[128];
    if (!bm_bili_get_wbi_keys(login, wbi_key, sizeof wbi_key, err)) {
        return false;
    }

    int64_t wts = bm_now_ms() / 1000;
    const char *dev_id = get_message_dev_id(login);
    char target_str[32];
    snprintf(target_str, sizeof target_str, "%llu", (unsigned long long)target_uid);

    bm_kv_pair wbi_params[3] = {
        { "w_sender_uid", login->DedeUserID },
        { "w_receiver_id", target_str },
        { "w_dev_id", dev_id }
    };
    char signed_query[1024];
    bm_bili_wbi_sign(wbi_params, 3, wbi_key, wts, signed_query, sizeof signed_query);

    cJSON *content_obj = cJSON_CreateObject();
    cJSON_AddStringToObject(content_obj, "content", content ? content : "");
    char *content_json = cJSON_PrintUnformatted(content_obj);
    cJSON_Delete(content_obj);

    char enc_content[8192];
    bm_url_encode(content_json ? content_json : "{\"content\":\"\"}", enc_content, sizeof enc_content);
    free(content_json);

    char enc_sender[128], enc_dev[128], enc_csrf[256];
    bm_url_encode(login->DedeUserID, enc_sender, sizeof enc_sender);
    bm_url_encode(dev_id, enc_dev, sizeof enc_dev);
    bm_url_encode(login->bili_jct, enc_csrf, sizeof enc_csrf);

    char post_data[10240];
    snprintf(
        post_data,
        sizeof post_data,
        "msg%%5Bsender_uid%%5D=%s&msg%%5Breceiver_type%%5D=1&msg%%5Breceiver_id%%5D=%s"
        "&msg%%5Bmsg_type%%5D=1&msg%%5Bmsg_status%%5D=0&msg%%5Bcontent%%5D=%s"
        "&msg%%5Bnew_face_version%%5D=0&msg%%5Bcanal_token%%5D=&msg%%5Bdev_id%%5D=%s"
        "&msg%%5Btimestamp%%5D=%lld&from_firework=0&build=0&mobi_app=web&csrf=%s",
        enc_sender,
        target_str,
        enc_content,
        enc_dev,
        (long long)wts,
        enc_csrf
    );

    return handle_im_message(login, post_data, signed_query, out_msg_key, key_cap, err);
}

bool bm_bili_recall_message(
    const bm_login_info *login,
    uint64_t target_uid,
    const char *msg_key,
    bm_bili_err *err
) {
    if (!bm_login_is_complete(login)) {
        set_err(err, -1, "登录信息不完整，请退出账号后重新扫码登录");
        return false;
    }
    const char *dev_id = get_message_dev_id(login);
    int64_t ts_ms = (bm_now_ms() / 1000) * 1000;

    char enc_sender[128], enc_key[128], enc_dev[128], enc_csrf[256];
    bm_url_encode(login->DedeUserID, enc_sender, sizeof enc_sender);
    bm_url_encode(msg_key ? msg_key : "", enc_key, sizeof enc_key);
    bm_url_encode(dev_id, enc_dev, sizeof enc_dev);
    bm_url_encode(login->bili_jct, enc_csrf, sizeof enc_csrf);

    char post_data[2048];
    snprintf(
        post_data,
        sizeof post_data,
        "msg%%5Bsender_uid%%5D=%s&msg%%5Breceiver_id%%5D=%llu&msg%%5Breceiver_type%%5D=1"
        "&msg%%5Bmsg_type%%5D=5&msg%%5Bmsg_status%%5D=0&msg%%5Bcontent%%5D=%s"
        "&msg%%5Btimestamp%%5D=%lld&msg%%5Bnew_face_version%%5D=0&msg%%5Bdev_id%%5D=%s"
        "&from_firework=0&build=0&csrf_token=%s&csrf=%s",
        enc_sender,
        (unsigned long long)target_uid,
        enc_key,
        (long long)ts_ms,
        enc_dev,
        enc_csrf,
        enc_csrf
    );

    return handle_im_message(login, post_data, NULL, NULL, 0, err);
}

static bool get_received_gifts_once(
    const bm_login_info *login,
    const char *cookie,
    const char *csrf,
    const char *begin_date,
    const char *end_date,
    int page,
    cJSON **out_root,
    const cJSON **out_data,
    bm_bili_err *err
) {
    *out_root = NULL;
    *out_data = NULL;
    (void)login;

    char enc_csrf[256];
    bm_url_encode(csrf, enc_csrf, sizeof enc_csrf);

    char post_data[512];
    snprintf(
        post_data,
        sizeof post_data,
        "page=%d&gift_id=0&begin_date=%s&end_date=%s&uname=&goods_id=5%%2C6%%2C7&csrf_token=%s&csrf=%s",
        page,
        begin_date,
        end_date,
        enc_csrf,
        enc_csrf
    );

    bm_http_resp resp;
    bm_http_req req = {
        .url = "https://api.live.bilibili.com/xlive/revenue/v1/giftStream/getReceivedGiftStream",
        .method = "POST",
        .body = post_data,
        .cookie = cookie,
        .accept = "application/json, text/plain, */*",
        .content_type = "application/x-www-form-urlencoded",
        .origin = "https://link.bilibili.com",
        .referer = "https://link.bilibili.com/p/center/index",
        .user_agent = GIFT_STREAM_UA,
        .timeout_ms = 30000
    };

    if (!bm_http_request(&req, &resp)) {
        if (resp.timed_out) {
            set_err(err, -1, "礼物流水请求超时，请稍后再试");
        } else {
            set_err(err, -1, resp.error[0] ? resp.error : "网络请求失败");
        }
        bm_http_resp_free(&resp);
        return false;
    }

    const char *text = resp.data ? resp.data : "";
    while (*text && isspace((unsigned char)*text)) text++;
    if (*text == '<') {
        bm_http_resp_free(&resp);
        set_err(err, -1, "礼物流水接口返回了 HTML，请求被风控拦截");
        return false;
    }

    cJSON *root = cJSON_Parse(text);
    bm_http_resp_free(&resp);
    if (!root) {
        set_err(err, -1, "Unexpected JSON in giftStream response");
        return false;
    }

    const cJSON *code = cJSON_GetObjectItemCaseSensitive(root, "code");
    if (!cJSON_IsNumber(code) || code->valueint != 0) {
        int c = cJSON_IsNumber(code) ? code->valueint : -1;
        const cJSON *msg = cJSON_GetObjectItemCaseSensitive(root, "message");
        set_err(err, c, cJSON_IsString(msg) ? msg->valuestring : "礼物流水接口返回错误");
        cJSON_Delete(root);
        return false;
    }

    *out_root = root;
    *out_data = cJSON_GetObjectItemCaseSensitive(root, "data");
    return true;
}

static bool get_received_gift_page(
    const bm_login_info *login,
    const char *cookie,
    const char *csrf,
    const char *begin_date,
    const char *end_date,
    int page,
    cJSON **out_root,
    const cJSON **out_data,
    bm_bili_err *err
) {
    int64_t deadline = bm_now_ms() + GIFT_STREAM_QUERY_TIMEOUT_MS;
    int delay = 1000;

    while (bm_now_ms() < deadline) {
        cJSON *root = NULL;
        const cJSON *data = NULL;
        if (!get_received_gifts_once(login, cookie, csrf, begin_date, end_date, page, &root, &data, err)) {
            return false;
        }
        const cJSON *ready = cJSON_IsObject(data) ? cJSON_GetObjectItemCaseSensitive(data, "ready") : NULL;
        if (cJSON_IsNumber(ready) && ready->valueint == 1) {
            *out_root = root;
            *out_data = data;
            return true;
        }
        cJSON_Delete(root);
        if (!cJSON_IsNumber(ready) || ready->valueint != 0) {
            set_err(err, -1, "礼物流水接口返回了无效数据");
            return false;
        }
        if (bm_now_ms() + delay >= deadline) {
            break;
        }
        bm_sleep_ms(delay);
        delay = delay * 2;
        if (delay > 10000) delay = 10000;
    }
    set_err(err, -1, "礼物流水数据查询超时，请稍后再试");
    return false;
}

static const char *goods_id_to_name(int goods_id, const char *fallback) {
    switch (goods_id) {
        case 5: return "总督";
        case 6: return "提督";
        case 7: return "舰长";
        default: return (fallback && fallback[0]) ? fallback : "舰长";
    }
}

static bool get_received_guards_month(
    const bm_login_info *login,
    const char *cookie,
    const char *csrf,
    const char *begin_date,
    const char *end_date,
    bm_guard_list *out,
    bm_bili_err *err
) {
    int total_pages = 1;
    for (int page = 0; page < total_pages; page++) {
        cJSON *root = NULL;
        const cJSON *data = NULL;
        if (!get_received_gift_page(login, cookie, csrf, begin_date, end_date, page, &root, &data, err)) {
            return false;
        }
        const cJSON *list = cJSON_GetObjectItemCaseSensitive(data, "list");
        if (!cJSON_IsArray(list)) {
            cJSON_Delete(root);
            set_err(err, -1, "礼物流水接口返回了无效列表");
            return false;
        }
        if (page == 0) {
            const cJSON *tp = cJSON_GetObjectItemCaseSensitive(data, "total_page");
            if (!cJSON_IsNumber(tp) || tp->valueint < 0 || (double)tp->valueint != tp->valuedouble) {
                cJSON_Delete(root);
                set_err(err, -1, "礼物流水接口返回了无效分页");
                return false;
            }
            total_pages = tp->valueint;
        }
        const cJSON *gift = NULL;
        cJSON_ArrayForEach(gift, list) {
            if (!cJSON_IsObject(gift)) continue;
            bm_guard g;
            memset(&g, 0, sizeof g);
            bm_strlcpy(g.face, "./static/noface.jpg", sizeof g.face);
            const cJSON *uname = cJSON_GetObjectItemCaseSensitive(gift, "uname");
            const cJSON *name = cJSON_GetObjectItemCaseSensitive(gift, "name");
            const cJSON *goods_id = cJSON_GetObjectItemCaseSensitive(gift, "goods_id");
            const cJSON *uid = cJSON_GetObjectItemCaseSensitive(gift, "uid");
            const cJSON *time_it = cJSON_GetObjectItemCaseSensitive(gift, "time");

            if (cJSON_IsString(uname) && uname->valuestring) bm_strlcpy(g.username, uname->valuestring, sizeof g.username);
            int gid = cJSON_IsNumber(goods_id) ? goods_id->valueint : 7;
            const char *gname = goods_id_to_name(gid, cJSON_IsString(name) ? name->valuestring : NULL);
            g.guard_level = bm_gift_name_to_level(gname);
            g.is_alive = 0;
            g.rank = 1;
            g.ruid = 61639371ULL;
            if (cJSON_IsNumber(uid)) g.uid = (uint64_t)uid->valuedouble;
            else if (cJSON_IsString(uid) && uid->valuestring) g.uid = (uint64_t)strtoull(uid->valuestring, NULL, 10);
            if (cJSON_IsString(time_it) && time_it->valuestring) bm_strlcpy(g.time, time_it->valuestring, sizeof g.time);
            bm_guard_list_push(out, &g);
        }
        cJSON_Delete(root);
    }
    return true;
}

bool bm_bili_get_received_guards_by_period(
    const bm_login_info *login,
    const char *begin_time,
    const char *end_time,
    bm_guard_list *out,
    bm_bili_err *err
) {
    bm_guard_list_clear(out);

    char cookie[2048];
    char csrf[256];
    bm_bili_cookie_string(login, cookie, sizeof cookie);
    bm_bili_extract_csrf(login, csrf, sizeof csrf);
    if (!csrf[0]) {
        set_err(err, -1, "登录信息不完整，请退出账号后重新扫码登录");
        return false;
    }

    int by = 0, bm = 0, bd = 0;
    int ey = 0, em = 0, ed = 0;
    if (!bm_parse_ymd(begin_time, &by, &bm, &bd) || !bm_parse_ymd(end_time, &ey, &em, &ed)) {
        set_err(err, -1, "请选择有效的日期范围");
        return false;
    }
    if (by > ey || (by == ey && bm > em) || (by == ey && bm == em && bd > ed)) {
        set_err(err, -1, "开始日期不能晚于结束日期");
        return false;
    }

    int cy = by, cm = bm, cd = bd;
    while (cy < ey || (cy == ey && cm < em) || (cy == ey && cm == em && cd <= ed)) {
        int dim = bm_days_in_month(cy, cm);
        int ry = cy, rm = cm, rd = dim;
        if (ry > ey || (ry == ey && rm > em) || (ry == ey && rm == em && rd > ed)) {
            ry = ey;
            rm = em;
            rd = ed;
        }
        char b_str[16], e_str[16];
        snprintf(b_str, sizeof b_str, "%04d%02d%02d", cy, cm, cd);
        snprintf(e_str, sizeof e_str, "%04d%02d%02d", ry, rm, rd);

        if (!get_received_guards_month(login, cookie, csrf, b_str, e_str, out, err)) {
            bm_guard_list_clear(out);
            return false;
        }

        cy = (cm == 12) ? (cy + 1) : cy;
        cm = (cm == 12) ? 1 : (cm + 1);
        cd = 1;
    }
    return true;
}

bool bm_bili_qr_generate(
    char *out_url,
    size_t url_cap,
    char *out_key,
    size_t key_cap,
    bm_bili_err *err
) {
    if (out_url && url_cap) out_url[0] = '\0';
    if (out_key && key_cap) out_key[0] = '\0';

    bm_http_resp resp;
    bm_http_req req = {
        .url = "https://passport.bilibili.com/x/passport-login/web/qrcode/generate",
        .method = "GET"
    };
    if (!bm_http_request(&req, &resp)) {
        set_err(err, -1, resp.error);
        bm_http_resp_free(&resp);
        return false;
    }

    cJSON *root = cJSON_Parse(resp.data ? resp.data : "");
    bm_http_resp_free(&resp);
    if (!root) {
        set_err(err, -1, "解析二维码生成结果失败");
        return false;
    }

    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    const cJSON *url = data ? cJSON_GetObjectItemCaseSensitive(data, "url") : NULL;
    const cJSON *key = data ? cJSON_GetObjectItemCaseSensitive(data, "qrcode_key") : NULL;
    if (!cJSON_IsString(url) || !cJSON_IsString(key)) {
        set_err(err, -1, "未能获取有效的登录二维码");
        cJSON_Delete(root);
        return false;
    }

    bm_strlcpy(out_url, url->valuestring, url_cap);
    bm_strlcpy(out_key, key->valuestring, key_cap);
    cJSON_Delete(root);
    return true;
}

static void extract_query_param(const char *url, const char *key, char *out, size_t cap) {
    const char *q = strchr(url, '?');
    if (!q) return;
    q++;
    size_t klen = strlen(key);
    while (*q) {
        const char *amp = strchr(q, '&');
        size_t plen = amp ? (size_t)(amp - q) : strlen(q);
        if (plen > klen + 1 && strncmp(q, key, klen) == 0 && q[klen] == '=') {
            char raw_val[512];
            size_t vlen = plen - (klen + 1);
            if (vlen >= sizeof raw_val) vlen = sizeof raw_val - 1;
            memcpy(raw_val, q + klen + 1, vlen);
            raw_val[vlen] = '\0';
            const char *hash = strchr(raw_val, '#');
            if (hash) raw_val[hash - raw_val] = '\0';
            bm_strlcpy(out, raw_val, cap);
            return;
        }
        q = amp ? amp + 1 : q + plen;
    }
}

static void populate_login_from_cookie_map(const cookie_map *m, bm_login_info *login) {
    memset(login, 0, sizeof *login);
    const char *sess = cookie_map_get(m, "SESSDATA");
    const char *uid = cookie_map_get(m, "DedeUserID");
    const char *md5 = cookie_map_get(m, "DedeUserID__ckMd5");
    const char *jct = cookie_map_get(m, "bili_jct");
    const char *sid = cookie_map_get(m, "sid");
    if (sess) bm_normalize_sessdata(sess, login->SESSDATA, sizeof login->SESSDATA);
    if (uid) bm_strlcpy(login->DedeUserID, uid, sizeof login->DedeUserID);
    if (md5) bm_strlcpy(login->DedeUserID__ckMd5, md5, sizeof login->DedeUserID__ckMd5);
    if (jct) bm_strlcpy(login->bili_jct, jct, sizeof login->bili_jct);
    if (sid) bm_strlcpy(login->sid, sid, sizeof login->sid);

    size_t pos = 0;
    for (int i = 0; i < m->count; i++) {
        int n = snprintf(login->cookies + pos, sizeof login->cookies - pos, "%s%s=%s",
                         i > 0 ? "; " : "", m->items[i].key, m->items[i].val);
        if (n > 0 && pos + (size_t)n < sizeof login->cookies) pos += (size_t)n;
    }
    login->has_login = bm_login_is_complete(login);
}

static bool resolve_qr_login(const char *login_url, bm_login_info *out_login, bm_bili_err *err) {
    cookie_map cookies;
    memset(&cookies, 0, sizeof cookies);

    static const char *keys[] = { "SESSDATA", "DedeUserID", "DedeUserID__ckMd5", "bili_jct", "sid" };
    char cur_url[1024];
    bm_strlcpy(cur_url, login_url, sizeof cur_url);

    for (int redirects = 0; redirects <= 5; redirects++) {
        for (size_t i = 0; i < sizeof keys / sizeof keys[0]; i++) {
            char val[512] = { 0 };
            extract_query_param(cur_url, keys[i], val, sizeof val);
            if (val[0]) cookie_map_set(&cookies, keys[i], val);
        }
        populate_login_from_cookie_map(&cookies, out_login);
        if (out_login->has_login) return true;

        if (strncmp(cur_url, "https://", 8) != 0) {
            set_err(err, -1, "扫码登录跳转到了不安全的地址");
            return false;
        }

        bm_http_resp resp;
        bm_http_req req = {
            .url = cur_url,
            .method = "GET",
            .cookie = out_login->cookies,
            .referer = "https://passport.bilibili.com/",
            .user_agent = DEFAULT_BILI_UA,
            .no_follow_redirect = true
        };
        if (!bm_http_request(&req, &resp)) {
            set_err(err, -1, resp.error);
            bm_http_resp_free(&resp);
            return false;
        }

        for (int i = 0; i < resp.set_cookie_count; i++) {
            char pair[512];
            bm_strlcpy(pair, resp.set_cookies[i], sizeof pair);
            char *semi = strchr(pair, ';');
            if (semi) *semi = '\0';
            char *eq = strchr(pair, '=');
            if (eq && eq != pair) {
                *eq = '\0';
                cookie_map_set(&cookies, bm_trim(pair), bm_trim(eq + 1));
            }
        }
        populate_login_from_cookie_map(&cookies, out_login);
        if (out_login->has_login) {
            bm_http_resp_free(&resp);
            return true;
        }

        if (!resp.location[0]) {
            bm_http_resp_free(&resp);
            break;
        }
        bm_strlcpy(cur_url, resp.location, sizeof cur_url);
        bm_http_resp_free(&resp);
    }

    set_err(err, -1, "登录成功，但未能取得账号 Cookie，请重新扫码");
    return false;
}

bool bm_bili_qr_poll(
    const char *qrcode_key,
    int *out_status_code,
    char *out_status_msg,
    size_t msg_cap,
    bm_login_info *out_login,
    bm_bili_err *err
) {
    if (out_status_code) *out_status_code = -1;
    if (out_status_msg && msg_cap) out_status_msg[0] = '\0';

    char url[256];
    snprintf(url, sizeof url, "https://passport.bilibili.com/x/passport-login/web/qrcode/poll?qrcode_key=%s", qrcode_key);

    bm_http_resp resp;
    bm_http_req req = { .url = url, .method = "GET" };
    if (!bm_http_request(&req, &resp)) {
        set_err(err, -1, resp.error);
        bm_http_resp_free(&resp);
        return false;
    }

    for (int i = 0; i < resp.set_cookie_count && out_login; i++) {
        (void)i;
    }

    cJSON *root = cJSON_Parse(resp.data ? resp.data : "");
    bm_http_resp_free(&resp);
    if (!root) {
        set_err(err, -1, "解析扫码状态失败");
        return false;
    }

    const cJSON *data = cJSON_GetObjectItemCaseSensitive(root, "data");
    const cJSON *code = data ? cJSON_GetObjectItemCaseSensitive(data, "code") : NULL;
    const cJSON *login_url = data ? cJSON_GetObjectItemCaseSensitive(data, "url") : NULL;
    if (!cJSON_IsNumber(code)) {
        set_err(err, -1, "扫码状态响应无效");
        cJSON_Delete(root);
        return false;
    }

    int sc = code->valueint;
    if (out_status_code) *out_status_code = sc;

    if (sc == 0) {
        const char *u = cJSON_IsString(login_url) ? login_url->valuestring : "";
        bool ok = resolve_qr_login(u, out_login, err);
        if (ok) {
            bm_strlcpy(out_status_msg, "登录成功", msg_cap);
        } else {
            snprintf(out_status_msg, msg_cap, "登录失败：%s", err ? err->message : "");
        }
        cJSON_Delete(root);
        return ok;
    } else if (sc == 86101) {
        bm_strlcpy(out_status_msg, "请使用 bilibili 手机 App 扫描下方二维码", msg_cap);
    } else if (sc == 86090) {
        bm_strlcpy(out_status_msg, "请在手机上点击 确认登录", msg_cap);
    } else if (sc == 86038) {
        bm_strlcpy(out_status_msg, "二维码已失效，请重新打开扫码窗口", msg_cap);
    } else {
        const cJSON *m = data ? cJSON_GetObjectItemCaseSensitive(data, "message") : NULL;
        snprintf(out_status_msg, msg_cap, "确认状态失败 (%d): %s", sc, cJSON_IsString(m) ? m->valuestring : "");
    }
    cJSON_Delete(root);
    return true;
}

bool bm_bili_check_github_update(
    char *out_tag,
    size_t tag_cap,
    char *out_body,
    size_t body_cap,
    char *out_html_url,
    size_t url_cap,
    bm_bili_err *err
) {
    if (out_tag && tag_cap) out_tag[0] = '\0';
    if (out_body && body_cap) out_body[0] = '\0';
    if (out_html_url && url_cap) out_html_url[0] = '\0';

    bm_http_resp resp;
    bm_http_req req = {
        .url = "https://api.github.com/repos/xinrea/BiliMessengerElectron/releases/latest",
        .method = "GET",
        .user_agent = "BiliMessenger",
        .timeout_ms = 10000
    };
    if (!bm_http_request(&req, &resp)) {
        set_err(err, -1, resp.error);
        bm_http_resp_free(&resp);
        return false;
    }

    cJSON *root = cJSON_Parse(resp.data ? resp.data : "");
    bm_http_resp_free(&resp);
    if (!root) {
        set_err(err, -1, "解析版本信息失败");
        return false;
    }

    const cJSON *tag = cJSON_GetObjectItemCaseSensitive(root, "tag_name");
    const cJSON *body = cJSON_GetObjectItemCaseSensitive(root, "body");
    const cJSON *html_url = cJSON_GetObjectItemCaseSensitive(root, "html_url");
    if (!cJSON_IsString(tag) || !tag->valuestring) {
        cJSON_Delete(root);
        set_err(err, -1, "未找到最新版本标签");
        return false;
    }

    bm_strlcpy(out_tag, tag->valuestring, tag_cap);
    if (cJSON_IsString(body) && body->valuestring) bm_strlcpy(out_body, body->valuestring, body_cap);
    if (cJSON_IsString(html_url) && html_url->valuestring) bm_strlcpy(out_html_url, html_url->valuestring, url_cap);
    cJSON_Delete(root);
    return true;
}
