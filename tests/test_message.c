/* Real page callbacks and roster refresh; jobs/network are deterministic. */
#include "ui/app.h"
#include <cui/cui_render.h>
#ifdef NDEBUG
#error "Message tests require assertions enabled"
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void *pending_job;
static bm_job_fn pending_worker;
static bm_main_cb pending_callback;
static bool spawn_ok = true;
static int network_sends;
static const char *import_path;

static bool test_spawn(bm_job_fn fn, void *arg) {
    if (!spawn_ok) return false;
    assert(!pending_job);
    pending_job = arg;
    pending_worker = fn;
    return true;
}

static void test_post(bm_main_cb fn, void *arg) {
    assert(arg == pending_job);
    pending_callback = fn;
}

static bool test_send(const bm_login_info *login, uint64_t uid, const char *message,
                      char *key, size_t cap, bm_bili_err *err) {
    assert(uid != 0);
    network_sends++;
    bm_strlcpy(key, "test-key", cap);
    return true;
}

#define bm_job_spawn test_spawn
#define bm_job_post_main test_post
#define bm_bili_send_message test_send
#include "../src/pages/message.c"
#include "../src/pages/guard_list.c"
#undef bm_job_spawn
#undef bm_job_post_main
#undef bm_bili_send_message

cui_texture bm_image_get(cui_ctx *ui, const char *url, int *w, int *h) { return 0; }
cui_texture bm_guard_icon_get(cui_ctx *ui, int level) { return 0; }
void bm_images_shutdown(cui_ctx *ui) {}
bool cui_app_file_dialog(cui_app *app, const cui_file_dialog_opts *opts, char *out, size_t cap) {
    assert(import_path);
    bm_strlcpy(out, import_path, cap);
    return true;
}
bool cui_app_open_url(const char *url) { assert(!"Unexpected URL"); return false; }

static bm_app *new_app(void) {
    bm_app *app = calloc(1, sizeof *app);
    assert(app);
    bm_store_init(&app->store);
    bm_strlcpy(app->store.path, "bm_test_message.json", sizeof app->store.path);
    bm_strlcpy(app->store.room_id, "123", sizeof app->store.room_id);
    app->store.login.has_login = true;
    app->filters = (bm_guard_filters){true, true, true};
    bm_store_set_template(&app->store, "test", "Hello {name}: {code}");
    assert(app->store.template_count == 1);
    app->msg_template_idx = 0;
    assert(bm_store_import_codes(&app->store, "CODE-A\nCODE-B\nCODE-C") == 3);
    bm_guard a = {.uid = 1, .username = "Alice", .guard_level = 3};
    bm_guard b = {.uid = 2, .username = "Bob", .guard_level = 3};
    bm_guard c = {.uid = 3, .username = "Governor", .guard_level = 1};
    assert(bm_guard_list_push(&app->store.original_guards, &a));
    assert(bm_guard_list_push(&app->store.original_guards, &b));
    assert(bm_guard_list_push(&app->store.original_guards, &c));
    bm_refresh_guard_stats_and_filters(app, false);
    return app;
}

static void destroy_app(bm_app *app) {
    assert(!pending_job && !app->msg_send_pending);
    bm_store_free(&app->store);
    bm_guard_list_free(&app->msg_remain_list);
    free(app->sent_msgs);
    free(app);
    assert(remove("bm_test_message.json") == 0);
}

static void start(bm_app *app) {
    app->msg_in_progress = true;
    app->msg_total_count = app->store.guards.count;
    trigger_next_send_step(app);
    assert(pending_job && app->msg_send_pending);
}

static void finish(bm_app *app, bool ok) {
    send_step_job *job = pending_job;
    assert(job);
    pending_job = NULL;
    job->ok = ok;
    on_send_step_done(job, app);
}

static void test_live_edits(void) {
    bm_app *app = new_app();
    start(app);
    finish(app, false); /* Alice failed; Bob is now in flight. */
    assert(((send_step_job *)pending_job)->target.uid == 2);
    assert(app->store.original_guards.count == 3);
    bm_refresh_guard_stats_and_filters(app, true);
    assert(app->store.guards.count == 2); /* Do not reinsert Alice. */
    app->filters.type1 = false;
    bm_refresh_guard_stats_and_filters(app, true);
    assert(app->store.guards.count == 1);
    bm_guard added = {.uid = 4, .username = "Added during send", .guard_level = 3};
    assert(bm_guard_list_push(&app->store.original_guards, &added));
    bm_refresh_guard_stats_and_filters(app, true);
    assert(app->store.guards.count == 1); /* New additions wait for the next pass. */
    /* Delete the in-flight recipient; it cannot cancel an already-issued request. */
    app->store.guards.items[0].selected = true;
    delete_selected_guards(app);
    assert(app->store.guards.count == 0 && app->stats.total == 3);
    finish(app, true);
    assert(!pending_job && !app->msg_in_progress);
    assert(app->sent_msg_count == 1 && app->store.code_count == 2);
    assert(app->store.guards.count == 2 && app->store.guards.items[0].uid == 1);
    assert(app->store.guards.items[1].uid == 4);
    assert(bm_guard_list_find_uid(&app->store.original_guards, 3) >= 0);
    destroy_app(app);

    app = new_app();
    start(app);
    bm_guard_list_clear(&app->store.original_guards);
    bm_refresh_guard_stats_and_filters(app, true);
    assert(app->store.guards.count == 0 && app->stats.total == 0);
    finish(app, true);
    assert(!pending_job && app->store.original_guards.count == 0);
    destroy_app(app);
}

static void test_replaced_roster(void) {
    /* Snapshot, gift stream and file import must all invalidate old callbacks. */
    for (int kind = 0; kind < 3; kind++) {
        for (int ok = 0; ok <= 1; ok++) {
            bm_app *app = new_app();
            start(app);
            bm_guard replacement = {.uid = 1, .username = "New Alice", .guard_level = 3};
            if (kind == 0) {
                snapshot_job_data *job = calloc(1, sizeof *job);
                assert(job);
                job->ok = true;
                assert(bm_guard_list_push(&job->list, &replacement));
                on_snapshot_loaded(job, app);
            } else if (kind == 1) {
                gift_job_data *job = calloc(1, sizeof *job);
                assert(job);
                job->ok = true;
                assert(bm_guard_list_push(&job->list, &replacement));
                on_gift_loaded(job, app);
            } else {
                import_path = "bm_test_message_import.csv";
                const char *csv = "舰长,1,NewAlice\n";
                assert(bm_write_file(import_path, csv, strlen(csv)));
                open_import_file_dialog(app);
                assert(remove(import_path) == 0);
                import_path = NULL;
            }
            assert(app->guard_roster_generation == 1);
            assert(!app->msg_in_progress && app->msg_send_pending);
            assert(app->store.guards.count == 1);
            finish(app, ok != 0);
            assert(!pending_job && app->store.original_guards.count == 1);
            assert(app->store.original_guards.items[0].uid == 1);
            assert(app->store.guards.count == 1);
            assert(app->store.code_count == 3 - ok);
            assert(app->sent_msg_count == ok);
            bm_store loaded;
            bm_store_init(&loaded);
            assert(bm_store_load(&loaded, app->store.path));
            assert(loaded.original_guards.count == 1 && loaded.code_count == 3 - ok);
            bm_store_free(&loaded);
            destroy_app(app);
        }
    }
}

static void test_pause_and_invalid_uid(void) {
    bm_app *app = new_app();
    start(app);
    void *first = pending_job;
    app->msg_in_progress = false;
    trigger_next_send_step(app);
    assert(pending_job == first); /* No second chain before the callback. */
    finish(app, true);
    assert(!pending_job && app->store.guards.count == 2);
    start(app);
    assert(((send_step_job *)pending_job)->target.uid == 2);
    app->msg_in_progress = false;
    finish(app, false);
    assert(app->store.guards.count == 2);
    destroy_app(app);

    app = new_app();
    bm_guard invalid = {.uid = 0, .username = "Invalid"};
    assert(bm_guard_list_unshift(&app->store.original_guards, &invalid));
    assert(bm_guard_list_unshift(&app->store.original_guards, &invalid));
    bm_refresh_guard_stats_and_filters(app, false);
    start(app);
    for (int i = 0; i < 2; i++) {
        send_step_job *job = pending_job;
        assert(job->target.uid == 0);
        job->interval_ms = 0;
        pending_worker(job);
        assert(pending_callback == on_send_step_done && !job->ok && network_sends == 0);
        pending_job = NULL;
        pending_callback(job, app);
        pending_callback = NULL;
    }
    assert(((send_step_job *)pending_job)->target.uid == 1);
    assert(app->store.original_guards.count == 5 && app->store.code_count == 3);
    app->msg_in_progress = false;
    finish(app, true);
    assert(app->store.original_guards.count == 4);
    destroy_app(app);
}

static void test_spawn_failure_and_code_exhaustion(void) {
    bm_app *app = new_app();
    spawn_ok = false;
    app->msg_in_progress = true;
    trigger_next_send_step(app);
    assert(!app->msg_in_progress && !app->msg_send_pending && !pending_job);
    assert(app->store.guards.count == 3);
    spawn_ok = true;
    start(app);
    bm_store_clear_codes(&app->store);
    finish(app, true);
    assert(!app->msg_in_progress && !app->msg_send_pending && !pending_job);
    assert(app->store.guards.count == 2 && app->store.original_guards.count == 2);
    destroy_app(app);
}

static void test_refresh_during_recall(void) {
    bm_app *app = new_app();
    app->msg_in_progress = true;
    app->msg_is_recalling = true;
    app->msg_progress_index = 2;
    app->msg_total_count = 5;
    bm_guard_list_clear(&app->store.original_guards);
    bm_guard replacement = {.uid = 99, .username = "New roster"};
    assert(bm_guard_list_push(&app->store.original_guards, &replacement));
    bm_guard_roster_replaced(app);
    assert(app->msg_in_progress && app->msg_is_recalling);
    assert(app->msg_progress_index == 2 && app->msg_total_count == 5);
    assert(app->store.guards.count == 1 && app->store.guards.items[0].uid == 99);
    destroy_app(app);
}

static void frame(cui_ctx *ui, bm_app *app) {
    for (int i = 0; i < 4; i++) {
        cui_begin_frame(ui, 1.0f / 60.0f);
        bm_page_message(ui, app);
        cui_end_frame(ui);
    }
}

static void click(cui_ctx *ui, bm_app *app, const char *label) {
    cui_rect rect = cui_get_rect(ui, cui_make_id(ui, label));
    assert(rect.w > 0 && rect.h > 0);
    cui_input_mouse_move(ui, rect.x + rect.w / 2, rect.y + rect.h / 2);
    frame(ui, app);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, true);
    frame(ui, app);
    cui_input_mouse_button(ui, CUI_MOUSE_LEFT, false);
    frame(ui, app);
}

static void test_pause_controls(void) {
    const int width = 1280, height = 760;
    cui_sw_renderer *renderer = cui_sw_create();
    cui_ctx *ui = cui_create(&(cui_ctx_desc){.renderer = cui_sw_iface(renderer), .theme = CUI_THEME_LIGHT});
    assert(ui);
    bm_fonts_init(ui);
    cui_set_viewport(ui, width, height, 1);
    cui_set_reduced_motion(ui, true);
    bm_app *app = new_app();
    bm_guard sent = {.uid = 99, .username = "Previously sent"};
    record_sent_msg(app, &sent, "old-key", bm_now_ms());
    start(app);
    frame(ui, app);
    click(ui, app, "暂停群发");
    assert(!app->msg_in_progress && app->msg_send_pending);

    /* Optional actual page render, with identical fixture/viewport for PR evidence. */
    const char *path = getenv("BM_MESSAGE_SCREENSHOT");
    if (path && path[0]) {
        void *pixels = calloc((size_t)width * height, 4);
        assert(pixels);
        cui_sw_render(renderer, cui_get_draw_data(ui), pixels, width, height, width * 4, CUI_SW_RGBA);
        assert(cui_write_png(path, pixels, width, height, width * 4));
        free(pixels);
    }

    void *first = pending_job;
    click(ui, app, "开始群发私信");
    assert(pending_job == first && !app->msg_in_progress);
    click(ui, app, "一键撤回全部消息");
    assert(pending_job == first && !app->msg_in_progress && !app->msg_is_recalling);
    click(ui, app, "跳过队首用户");
    assert(app->store.original_guards.count == 3);
    finish(app, true);
    frame(ui, app);
    click(ui, app, "开始群发私信");
    assert(app->msg_in_progress && app->msg_send_pending);
    app->msg_in_progress = false;
    finish(app, true);
    destroy_app(app);
    cui_destroy(ui);
    cui_sw_destroy(renderer);
}

int main(void) {
    test_live_edits();
    test_replaced_roster();
    test_pause_and_invalid_uid();
    test_spawn_failure_and_code_exhaustion();
    test_refresh_during_recall();
    test_pause_controls();
    puts("PASS send callbacks, live roster edits, imports, pause, invalid UID and stop paths");
    return 0;
}
