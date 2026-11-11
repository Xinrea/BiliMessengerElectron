#ifndef BM_APP_H
#define BM_APP_H

#include "core/bili.h"
#include "core/guards.h"
#include "core/jobs.h"
#include "core/store.h"
#include "core/util.h"
#include "ui/fonts.h"
#include "ui/images.h"
#include <cui/cui.h>
#include <cui/cui_app.h>

typedef enum bm_page_id {
    BM_PAGE_SUMMARY = 0,
    BM_PAGE_GUARD_LIST = 1,
    BM_PAGE_TEMPLATE = 2,
    BM_PAGE_CODES = 3,
    BM_PAGE_MESSAGE = 4,
    BM_PAGE_SETTING = 5,
    BM_PAGE_ABOUT = 6,
    BM_PAGE_COUNT = 7
} bm_page_id;

typedef struct bm_sent_msg {
    uint64_t uid;
    char username[128];
    char msg_key[64];
    int64_t timestamp_ms;
} bm_sent_msg;

typedef struct bm_pending_toast {
    char title[128];
    char desc[256];
    cui_tone tone;
} bm_pending_toast;

typedef struct bm_app {
    bm_store store;
    bm_page_id page;
    bool sidebar_mini;
    bool fonts_loaded;
    bool theme_applied;

    /* Pending toasts from callbacks */
    bm_pending_toast toasts[8];
    int toast_count;

    /* Summary page */
    bm_room_info room_info;
    bm_chart_history guard_history;
    bm_chart_history follower_history;
    bool summary_loading;
    bool summary_loaded;

    /* Guard list page */
    bm_guard_stats stats;
    bm_guard_filters filters;
    cui_date snapshot_date;
    cui_date_range gift_range;
    bm_date_list valid_dates;
    bool guard_loading;
    bool add_guard_dialog;
    bool add_guard_by_username;
    char add_guard_keyword[128];
    char add_guard_title[64];

    /* Template page */
    int template_select_idx;
    char template_name_buf[128];
    char template_value_buf[4096];
    bool template_changed;

    /* Code manage page */
    bool add_code_dialog;
    char code_import_buf[16384];

    /* Message page */
    int msg_template_idx;
    char msg_template_name[128];
    bool msg_in_progress;
    int msg_progress_index;
    int msg_total_count;
    bm_guard_list msg_remain_list;
    bm_sent_msg *sent_msgs;
    int sent_msg_count;
    int sent_msg_cap;

    /* Setting page */
    bm_user_profile account_profile;
    char room_input[64];
    bool room_edited;
    bool room_saving;
    char interval_input[32];
    bool login_dialog;
    bool login_step_scan;
    bool manual_login_enabled;
    char manual_sessdata[512];
    char manual_uid[64];
    char manual_jct[128];
    char qr_url[512];
    char qr_key[128];
    char qr_status[256];
    cui_texture qr_tex;
    bool qr_tex_dirty;
    bool qr_polling;
    int64_t qr_last_poll_ms;

    /* Update check */
    bool update_dialog;
    char update_tag[64];
    char update_body[2048];
    char update_url[512];
} bm_app;

void bm_app_init(bm_app *app);
void bm_app_shutdown(bm_app *app, cui_ctx *ui);

void bm_toast_push(bm_app *app, const char *title, const char *desc, cui_tone tone);
void bm_toast_flush(bm_app *app, cui_ctx *ui);

void bm_render_setting_alert(cui_ctx *ui, bm_app *app);
void bm_refresh_guard_stats_and_filters(bm_app *app, bool save_to_disk);

/* Page renderers & actions */
void bm_page_summary(cui_ctx *ui, bm_app *app);
void bm_summary_refresh(bm_app *app);

void bm_page_guard_list(cui_ctx *ui, bm_app *app);
void bm_guard_list_refresh_all(bm_app *app);
void bm_guard_list_refresh_calendar(bm_app *app);
void bm_guard_list_fetch_snapshot(bm_app *app, const char *date_str);

void bm_page_template(cui_ctx *ui, bm_app *app);
void bm_page_codes(cui_ctx *ui, bm_app *app);
void bm_page_message(cui_ctx *ui, bm_app *app);

void bm_page_setting(cui_ctx *ui, bm_app *app);
void bm_setting_refresh_account(bm_app *app);
void bm_setting_startup_checks(bm_app *app);

void bm_page_about(cui_ctx *ui, bm_app *app);

#endif /* BM_APP_H */
