/* Exercise the real update controller and UI without network or browser side effects. */
#include "ui/app.h"
#include <cui/cui_render.h>

#ifdef NDEBUG
#error "Update tests require assertions enabled"
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bm_job_fn worker;
static bm_main_cb completion;
static void *job_data;
static bool spawn_ok = true, request_ok = true;
static int spawn_count, notice_count, open_count;
static cui_tone notice_tone;
static char notice[256], opened_url[512];
static const char *release_tag = "v9999.0.0";
static const char *release_url = "https://github.com/Xinrea/bili-messenger/releases/tag/v9999.0.0";

bool bm_job_spawn(bm_job_fn fn, void *data) {
    spawn_count++;
    if (!spawn_ok) return false;
    assert(!worker && !completion);
    worker = fn;
    job_data = data;
    return true;
}

void bm_job_post_main(bm_main_cb cb, void *data) {
    completion = cb;
    assert(data == job_data);
}

bool bm_bili_check_github_update(char *tag, size_t tag_cap, char *body, size_t body_cap,
                                char *url, size_t url_cap, bm_bili_err *err) {
    (void)body; (void)body_cap; (void)err;
    bm_strlcpy(tag, release_tag, tag_cap);
    bm_strlcpy(url, release_url, url_cap);
    return request_ok;
}

void bm_toast_push(bm_app *app, const char *title, const char *desc, cui_tone tone) {
    (void)app; (void)desc;
    notice_count++;
    notice_tone = tone;
    bm_strlcpy(notice, title, sizeof notice);
}

bool cui_app_open_url(const char *url) {
    open_count++;
    bm_strlcpy(opened_url, url, sizeof opened_url);
    return true;
}

static void finish_check(bm_app *app) {
    assert(worker && app->update_checking);
    worker(job_data);
    worker = NULL;
    assert(completion && app->update_checking);
    completion(job_data, app);
    completion = NULL;
    job_data = NULL;
    assert(!app->update_checking);
}

static void frame(cui_ctx *ui, bm_app *app, int count) {
    for (int i = 0; i < count; i++) {
        cui_begin_frame(ui, 1.0f / 60.0f);
        bm_page_about(ui, app);
        bm_render_update_toast(ui, app);
        cui_end_frame(ui);
    }
}

static void click(cui_ctx *ui, bm_app *app, cui_rect rect) {
    assert(rect.w > 0 && rect.h > 0);
    cui_input_mouse_move(ui, rect.x + rect.w / 2, rect.y + rect.h / 2);
    frame(ui, app, 2);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
    frame(ui, app, 1);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
    frame(ui, app, 2);
}

static void test_interaction(int width, int height, cui_theme_mode theme) {
    cui_sw_renderer *renderer = cui_sw_create();
    cui_ctx *ui = cui_create(&(cui_ctx_desc){.renderer = cui_sw_iface(renderer), .theme = theme});
    assert(ui);
    cui_set_viewport(ui, (float)width, (float)height, 1);
    cui_set_reduced_motion(ui, true);
    if (!bm_fonts_init(ui))
        puts("CJK font unavailable; using CUI fallback metrics");
    bm_app *app = calloc(1, sizeof *app);
    assert(app);
    frame(ui, app, 4);
    cui_rect button = cui_get_rect(ui, cui_make_id(ui, "检查更新"));
    assert(button.x >= 0 && button.y >= 0);
    assert(button.x + button.w <= width && button.y + button.h <= height);
    int before = spawn_count;
    click(ui, app, button);
    assert(app->update_checking && spawn_count == before + 1);
    click(ui, app, button);
    assert(spawn_count == before + 1); /* Loading button cannot start another request. */
    finish_check(app);
    assert(app->update_toast_pending);
    frame(ui, app, 4);
    assert(!app->update_toast_pending && app->update_toast_handle);
    assert(!open_count);

    int old_handle = app->update_toast_handle;
    bm_update_check(app, true);
    finish_check(app);
    frame(ui, app, 4);
    assert(app->update_toast_handle && app->update_toast_handle != old_handle);
    assert(!open_count); /* Rechecking replaces the toast without opening a browser. */

    /* The pinned CUI toast scopes its action button under the toast handle. */
    cui_push_id_int(ui, app->update_toast_handle);
    cui_rect action = cui_get_rect(ui, cui_make_id(ui, "查看更新"));
    cui_pop_id(ui);
    assert(action.x >= 0 && action.x + action.w <= width);
    assert(action.y >= 0 && action.y + action.h <= height);
    click(ui, app, action);
    assert(open_count == 1 && !strcmp(opened_url, release_url));
    assert(!app->update_toast_handle);
    frame(ui, app, 4);
    assert(open_count == 1);
    open_count = 0;
    free(app);
    cui_destroy(ui);
    cui_sw_destroy(renderer);
}

static void test_results(void) {
    bm_app *app = calloc(1, sizeof *app);
    assert(app);
    /* Automatic checks are asynchronous, single-flight, and notify on updates only. */
    int before = spawn_count;
    bm_update_check(app, false);
    bm_update_check(app, true);
    assert(spawn_count == before + 1);
    finish_check(app);
    assert(app->update_toast_pending && !notice_count);
    assert(!strcmp(app->update_tag, release_tag));
    app->update_toast_pending = false;

    const char *no_updates[] = {BM_VERSION, "v0.0.1", "invalid-tag"};
    for (size_t i = 0; i < sizeof no_updates / sizeof no_updates[0]; i++) {
        release_tag = no_updates[i];
        bm_update_check(app, false);
        finish_check(app);
        assert(!app->update_toast_pending && !notice_count);
        bm_update_check(app, true);
        finish_check(app);
        assert(notice_count == 1);
        assert(notice_tone == (i == 2 ? CUI_TONE_WARNING : CUI_TONE_SUCCESS));
        notice_count = 0;
    }
    request_ok = false;
    bm_update_check(app, false);
    finish_check(app);
    assert(!notice_count && !app->update_toast_pending);
    bm_update_check(app, true);
    finish_check(app);
    assert(notice_count == 1 && notice_tone == CUI_TONE_WARNING);
    assert(strstr(notice, "检查更新失败"));
    notice_count = 0;
    request_ok = true;

    spawn_ok = false;
    bm_update_check(app, false);
    assert(!app->update_checking && !notice_count);
    bm_update_check(app, true);
    assert(!app->update_checking && notice_count == 1 && notice_tone == CUI_TONE_WARNING);
    notice_count = 0;
    spawn_ok = true;

    release_tag = "v9999.0.0";
    const char *bad_urls[] = {"", "file:///tmp/release", "https://example.com/releases/latest"};
    for (size_t i = 0; i < sizeof bad_urls / sizeof bad_urls[0]; i++) {
        release_url = bad_urls[i];
        bm_update_check(app, true);
        finish_check(app);
        assert(app->update_toast_pending);
        assert(!strcmp(app->update_url, "https://github.com/Xinrea/bili-messenger/releases/latest"));
    }
    free(app);
}

int main(void) {
    test_interaction(1024, 640, CUI_THEME_LIGHT);
    test_interaction(1280, 760, CUI_THEME_DARK);
    test_results();
    puts("PASS update checks, single-flight, manual feedback, About button and clickable release toast");
    return 0;
}
