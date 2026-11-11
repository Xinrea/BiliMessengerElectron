#ifndef BM_BILI_H
#define BM_BILI_H

#include "core/guards.h"
#include "core/store.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct bm_bili_err {
    int code;
    char message[512];
} bm_bili_err;

typedef struct bm_kv_pair {
    const char *key;
    const char *value;
} bm_kv_pair;

typedef struct bm_user_profile {
    uint64_t mid;
    char uname[128];
    char face[512];
} bm_user_profile;

typedef struct bm_room_info {
    uint64_t room_id;
    uint64_t uid;
    char title[256];
    char cover[512];
    int online;
    int live_status;
    int64_t live_start_time;
    char anchor_uname[128];
    char anchor_face[512];
    int attention;
} bm_room_info;

#define BM_MAX_HISTORY_POINTS 64

typedef struct bm_chart_history {
    char labels[BM_MAX_HISTORY_POINTS][16];
    float values[BM_MAX_HISTORY_POINTS];
    int count;
} bm_chart_history;

typedef struct bm_date_list {
    char (*dates)[16];
    int count;
    int cap;
} bm_date_list;

void bm_date_list_init(bm_date_list *list);
void bm_date_list_free(bm_date_list *list);
void bm_date_list_clear(bm_date_list *list);
bool bm_date_list_push(bm_date_list *list, const char *date);
bool bm_date_list_contains(const bm_date_list *list, const char *date);

void bm_bili_reset_caches(void);
void bm_bili_cookie_string(const bm_login_info *login, char *out, size_t cap);
void bm_bili_extract_csrf(const bm_login_info *login, char *out, size_t cap);

void bm_bili_wbi_sign(
    const bm_kv_pair *params,
    int count,
    const char *wbi_key,
    int64_t wts,
    char *out_query,
    size_t cap
);

bool bm_bili_get_wbi_keys(const bm_login_info *login, char *out_key, size_t cap, bm_bili_err *err);
bool bm_bili_check_cookies(const bm_login_info *login, bool *out_is_login, bm_bili_err *err);
bool bm_bili_get_user_info(const bm_login_info *login, uint64_t mid, bm_user_profile *out, bm_bili_err *err);
bool bm_bili_search_user(const bm_login_info *login, const char *username, bm_user_profile *out, bm_bili_err *err);
bool bm_bili_get_room_info(const char *room_id, bm_room_info *out, bm_bili_err *err);
bool bm_bili_get_vtbs_history(uint64_t uid, bool is_guard, bm_chart_history *out, bm_bili_err *err);
bool bm_bili_get_guard_list(uint64_t ruid, bm_guard_list *out, bm_bili_err *err);
bool bm_bili_get_guard_valid_dates(const char *room_id, bm_date_list *out, bm_bili_err *err);
bool bm_bili_get_guard_snapshot(const char *room_id, const char *date, bm_guard_list *out, bm_bili_err *err);

bool bm_bili_send_message(
    const bm_login_info *login,
    uint64_t target_uid,
    const char *content,
    char *out_msg_key,
    size_t key_cap,
    bm_bili_err *err
);

bool bm_bili_recall_message(
    const bm_login_info *login,
    uint64_t target_uid,
    const char *msg_key,
    bm_bili_err *err
);

bool bm_bili_get_received_guards_by_period(
    const bm_login_info *login,
    const char *begin_time,
    const char *end_time,
    bm_guard_list *out,
    bm_bili_err *err
);

bool bm_bili_qr_generate(
    char *out_url,
    size_t url_cap,
    char *out_key,
    size_t key_cap,
    bm_bili_err *err
);

bool bm_bili_qr_poll(
    const char *qrcode_key,
    int *out_status_code,
    char *out_status_msg,
    size_t msg_cap,
    bm_login_info *out_login,
    bm_bili_err *err
);

bool bm_bili_check_github_update(
    char *out_tag,
    size_t tag_cap,
    char *out_body,
    size_t body_cap,
    char *out_html_url,
    size_t url_cap,
    bm_bili_err *err
);

#endif /* BM_BILI_H */
