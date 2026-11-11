/* Exercise the real import dialog with a fixed clock and no network jobs. */
#include "ui/app.h"
#include <cui/cui_render.h>
/* Inspect actual line bounds in the pinned CUI layout, not just widget presence. */
#include <cui_internal.h>

#ifdef NDEBUG
#error "Guard import UI tests require assertions enabled"
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static cui_date today;
static int jobs_started;
static bool spawn_ok = true;
static void *pending_job;
static char last_toast[256];
static char last_toast_desc[256];

static void test_date_today(int *y, int *m, int *d) {
    *y = today.year;
    *m = today.month;
    *d = today.day;
}

static bool test_job_spawn(bm_job_fn fn, void *arg) {
    (void)fn;
    if (!spawn_ok) return false;
    assert(!pending_job);
    pending_job = arg;
    jobs_started++;
    return true;
}

/* Keep page-local helpers private in production; replace only external effects. */
#define bm_date_today test_date_today
#define bm_job_spawn test_job_spawn
#include "../src/pages/guard_list.c"
#undef bm_date_today
#undef bm_job_spawn

void bm_toast_push(bm_app *app, const char *title, const char *desc, cui_tone tone) {
    bm_strlcpy(last_toast, title, sizeof last_toast);
    bm_strlcpy(last_toast_desc, desc ? desc : "", sizeof last_toast_desc);
}
void bm_refresh_guard_stats_and_filters(bm_app *app, bool save) {}
void bm_render_setting_alert(cui_ctx *ui, bm_app *app) {}
void bm_guard_badge(cui_ctx *ui, const bm_guard *g) {}
bool bm_guard_filter_chip(cui_ctx *ui, const char *id, int level, const char *label, bool *active) {
    return false;
}
cui_texture bm_image_get(cui_ctx *ui, const char *url, int *w, int *h) { return 0; }
bool cui_app_file_dialog(cui_app *app, const cui_file_dialog_opts *opts, char *out, size_t cap) {
    assert(!"Unexpected file dialog");
    return false;
}
bool cui_app_open_url(const char *url) {
    assert(!"Unexpected external URL");
    return false;
}

static void assert_date(cui_date actual, cui_date expected) {
    assert(actual.year == expected.year && actual.month == expected.month && actual.day == expected.day);
}

static void test_presets(void) {
    bm_app *app = calloc(1, sizeof *app);
    assert(app);
    /* Ordinary days, leap day, month/year rollovers, and the first of a month. */
    const cui_date dates[] = {
        {2026, 11, 11}, {2026, 11, 2}, {2026, 1, 1},
        {2024, 3, 1}, {2025, 3, 1}, {2024, 2, 29}
    };
    for (size_t i = 0; i < sizeof dates / sizeof dates[0]; i++) {
        today = dates[i];
        int yesterday = cui_date_to_days(today) - 1;
        set_gift_range_preset(app, 2);
        assert_date(app->gift_range.start, cui_date_from_days(yesterday - 6));
        assert_date(app->gift_range.end, cui_date_from_days(yesterday));
        cui_date_range previous = app->gift_range;
        set_gift_range_preset(app, 0);
        if (today.day == 1) {
            assert_date(app->gift_range.start, previous.start);
            assert_date(app->gift_range.end, previous.end);
        } else {
            assert_date(app->gift_range.start, ((cui_date){today.year, today.month, 1}));
            assert_date(app->gift_range.end, cui_date_from_days(yesterday));
        }
        set_gift_range_preset(app, 1);
        cui_date last = cui_date_from_days(cui_date_to_days((cui_date){today.year, today.month, 1}) - 1);
        assert_date(app->gift_range.start, ((cui_date){last.year, last.month, 1}));
        assert_date(app->gift_range.end, last);
    }
    free(app);
}

static void frame(cui_ctx *ui, bm_app *app, int count) {
    for (int i = 0; i < count; i++) {
        cui_begin_frame(ui, 1.0f / 60.0f);
        bm_page_guard_list(ui, app);
        cui_end_frame(ui);
    }
}

static cui_rect dialog_rect(cui_ctx *ui, const char *label) {
    cui_id dialog = cui_make_id(ui, "获取与导入目标名单") ^ 0xD1A106u;
    cui_push_id_int(ui, (int)dialog);
    cui_id id = cui_make_id(ui, label);
    cui_pop_id(ui);
    return cui_get_rect(ui, id);
}

static void click(cui_ctx *ui, bm_app *app, const char *label) {
    cui_rect rect = dialog_rect(ui, label);
    assert(rect.w > 0 && rect.h > 0);
    cui_input_mouse_move(ui, rect.x + rect.w / 2, rect.y + rect.h / 2);
    frame(ui, app, 1);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
    frame(ui, app, 1);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
    frame(ui, app, 2);
}

static cui_node *find_text(cui_ctx *ui, const char *text) {
    for (int i = 0; i < ui->node_count; i++) {
        cui_node *node = &ui->nodes[i];
        if (node->text && strstr(node->text, text)) return node;
    }
    assert(!"Expected text was not rendered");
    return NULL;
}

static void assert_text_fits(cui_ctx *ui, cui_node *node, cui_rect bounds) {
    assert(node->rect.x >= bounds.x - 0.5f);
    assert(node->rect.x + node->rect.w <= bounds.x + bounds.w + 0.5f);
    assert(node->rect.y >= bounds.y - 0.5f);
    assert(node->rect.y + node->rect.h <= bounds.y + bounds.h + 0.5f);
    assert(node->text_lines > 0 && node->lines);
    for (int i = 0; i < node->text_lines; i++) {
        assert(node->lines[i].width <= node->rect.w + 0.5f);
    }
}

/* Optional software-rendered screenshots, never written into the source tree. */
static void screenshot(cui_ctx *ui, cui_sw_renderer *renderer, const char *name, int width, int height) {
    const char *dir = getenv("BM_UI_SCREENSHOT_DIR");
    if (!dir || !dir[0]) return;
    char path[1024];
    snprintf(path, sizeof path, "%s/%s-%dx%d.png", dir, name, width, height);
    void *pixels = calloc((size_t)width * height, 4);
    assert(pixels);
    cui_sw_render(renderer, cui_get_draw_data(ui), pixels, width, height, width * 4, CUI_SW_RGBA);
    assert(cui_write_png(path, pixels, width, height, width * 4));
    free(pixels);
}

static void test_dialog(int width, int height, cui_theme_mode theme, float font_scale) {
    cui_sw_renderer *renderer = cui_sw_create();
    cui_ctx *ui = cui_create(&(cui_ctx_desc){.renderer = cui_sw_iface(renderer), .theme = theme});
    assert(ui);
    bool cjk = bm_fonts_init(ui);
    puts(cjk ? "Using application CJK font for layout checks" : "NOTE: no CJK font found; layout uses fallback glyphs");
    cui_theme_get(ui)->font_scale = font_scale;
    cui_set_viewport(ui, (float)width, (float)height, 1);
    cui_set_reduced_motion(ui, true);
    bm_app *app = calloc(1, sizeof *app);
    assert(app);
    strcpy(app->store.room_id, "123");
    app->store.login.has_login = true;
    strcpy(app->store.login.DedeUserID, "456");
    app->room_info.room_id = 123456; /* Configured short ID must not be compared to the full ID. */
    app->room_info.uid = 456;
    app->import_guard_dialog = true;
    today = (cui_date){2026, 11, 11};
    frame(ui, app, 4);
    cui_node *description = find_text(ui, "从 B 站接口获取指定时间段内的全部上舰礼物记录");
    assert(strstr(description->text, "（含重复续费，拉取后可一键去重合并）"));
    assert(description->text_lines >= 2);
    cui_rect content = ui->nodes[description->parent].rect;
    assert_text_fits(ui, description, content);
    assert_text_fits(ui, find_text(ui, "接口仅支持昨天及以前的数据"), content);
    screenshot(ui, renderer, "gift-import", width, height);
    assert(app->import_guard_tab == 0);
    assert_date(app->gift_range.start, ((cui_date){2026, 11, 4}));
    assert_date(app->gift_range.end, ((cui_date){2026, 11, 10}));
    click(ui, app, "本月流水");
    assert_date(app->gift_range.start, ((cui_date){2026, 11, 1}));
    assert_date(app->gift_range.end, ((cui_date){2026, 11, 10}));
    click(ui, app, "上月流水");
    assert_date(app->gift_range.start, ((cui_date){2026, 10, 1}));
    assert_date(app->gift_range.end, ((cui_date){2026, 10, 31}));
    click(ui, app, "近 7 天流水");

    today = (cui_date){2026, 12, 1};
    frame(ui, app, 4);
    cui_date_range previous = app->gift_range;
    click(ui, app, "本月流水");
    assert_date(app->gift_range.start, previous.start);
    assert_date(app->gift_range.end, previous.end);

    /* Reject missing, reversed, invalid, today, and future dates without closing. */
    const cui_date_range invalid[] = {
        {{0}, {0}},
        {{2026, 11, 30}, {0}},
        {{2026, 11, 30}, {2026, 11, 29}},
        {{2026, 2, 30}, {2026, 3, 1}},
        {{2026, 11, 30}, {2026, 12, 1}},
        {{2026, 11, 30}, {2026, 12, 2}}
    };
    for (size_t i = 0; i < sizeof invalid / sizeof invalid[0]; i++) {
        app->gift_range = invalid[i];
        int before = jobs_started;
        assert(!start_fetch_gift_range(app));
        assert(jobs_started == before && !app->guard_loading);
    }
    app->gift_range = invalid[4];
    click(ui, app, "开始拉取区间上舰流水");
    assert(app->import_guard_dialog && !app->guard_loading);
    assert(strstr(last_toast, "昨天"));

    set_gift_range_preset(app, 2);
    app->store.login.has_login = false;
    assert(!start_fetch_gift_range(app));
    app->store.login.has_login = true;

    int before = jobs_started;
    app->room_info.uid = 789;
    click(ui, app, "开始拉取区间上舰流水");
    assert(app->import_guard_dialog && !app->guard_loading && jobs_started == before);
    assert(strstr(last_toast, "不属于当前登录账号"));
    assert(!strcmp(last_toast_desc, "请切换为主播账号或使用别的方法获取"));
    int toast = cui_toast(ui, last_toast, .description = last_toast_desc, .tone = CUI_TONE_WARNING);
    frame(ui, app, 4);
    cui_rect toast_bounds = cui_get_rect(ui, 0x70A57u);
    assert(toast_bounds.w > 0 && toast_bounds.x + toast_bounds.w <= width);
    assert_text_fits(ui, find_text(ui, last_toast), toast_bounds);
    assert_text_fits(ui, find_text(ui, last_toast_desc), toast_bounds);
    screenshot(ui, renderer, "gift-ownership-warning", width, height);
    cui_toast_dismiss(ui, toast);
    frame(ui, app, 4);
    app->room_info.uid = 0;
    assert(!start_fetch_gift_range(app));
    assert(strstr(last_toast, "无法确认直播间归属"));
    app->room_info.uid = 456;
    app->store.room_id[0] = '\0';
    assert(!start_fetch_gift_range(app));
    strcpy(app->store.room_id, "123");
    app->store.login.DedeUserID[0] = '\0';
    assert(!start_fetch_gift_range(app));
    assert(strstr(last_toast, "登录账号信息不完整"));
    strcpy(app->store.login.DedeUserID, "789"); /* Account switches must be checked afresh. */
    assert(!start_fetch_gift_range(app));
    assert(strstr(last_toast, "不属于当前登录账号"));
    assert(!app->guard_loading && jobs_started == before);
    strcpy(app->store.login.DedeUserID, "456");

    spawn_ok = false;
    assert(!start_fetch_gift_range(app) && !app->guard_loading);
    spawn_ok = true;
    click(ui, app, "开始拉取区间上舰流水");
    assert(!app->import_guard_dialog && app->guard_loading);
    gift_job_data *job = pending_job;
    assert(job && !strcmp(job->begin_date, "2026-11-24") && !strcmp(job->end_date, "2026-11-30"));
    assert(!start_fetch_gift_range(app)); /* no duplicate in-flight jobs */
    free(pending_job);
    pending_job = NULL;
    app->guard_loading = false;

    app->import_guard_dialog = true;
    app->room_info.uid = 789; /* Ownership restrictions apply only to gift history. */
    app->import_guard_tab = 1;
    frame(ui, app, 4);
    assert(dialog_rect(ui, "拉取所选日期快照").h > 0);
    assert(dialog_rect(ui, "拉取今日快照").h == 0);
    free(app);
    cui_destroy(ui);
    cui_sw_destroy(renderer);
}

int main(void) {
    test_presets();
    test_dialog(1024, 640, CUI_THEME_LIGHT, 1.0f);
    test_dialog(1280, 760, CUI_THEME_DARK, 1.25f);
    puts("PASS guard import defaults, presets, date and ownership validation, submission and snapshot UI");
    return 0;
}
