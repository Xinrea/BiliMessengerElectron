#include "ui/app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bm_app_init(bm_app *app) {
    memset(app, 0, sizeof *app);
    bm_store_init(&app->store);
    bm_store_load(&app->store, NULL);

    app->filters.type1 = true;
    app->filters.type2 = true;
    app->filters.type3 = true;

    bm_date_list_init(&app->valid_dates);
    bm_guard_list_init(&app->msg_remain_list);

    int y, m, d;
    bm_date_today(&y, &m, &d);
    app->snapshot_date.year = y;
    app->snapshot_date.month = m;
    app->snapshot_date.day = d;

    char today_str[16];
    snprintf(today_str, sizeof today_str, "%04d-%02d-%02d", y, m, d);
    bm_date_list_push(&app->valid_dates, today_str);

    bm_strlcpy(app->room_input, app->store.room_id, sizeof app->room_input);
    snprintf(app->interval_input, sizeof app->interval_input, "%d", app->store.send_interval);

    app->template_select_idx = -1;
    app->msg_template_idx = -1;
    if (app->store.template_count > 0) {
        app->template_select_idx = 0;
        bm_strlcpy(app->template_name_buf, app->store.templates[0].name, sizeof app->template_name_buf);
        bm_strlcpy(app->template_value_buf, app->store.templates[0].value, sizeof app->template_value_buf);
        app->msg_template_idx = 0;
        bm_strlcpy(app->msg_template_name, app->store.templates[0].name, sizeof app->msg_template_name);
    }

    bm_refresh_guard_stats_and_filters(app, false);
    app->msg_total_count = app->store.guards.count;
}

void bm_app_shutdown(bm_app *app, cui_ctx *ui) {
    if (!app) return;
    if (app->qr_tex && ui) {
        cui_image_destroy(ui, app->qr_tex);
        app->qr_tex = 0;
    }
    bm_images_shutdown(ui);
    bm_date_list_free(&app->valid_dates);
    bm_guard_list_free(&app->msg_remain_list);
    free(app->sent_msgs);
    app->sent_msgs = NULL;
    app->sent_msg_count = app->sent_msg_cap = 0;
    bm_store_free(&app->store);
}

void bm_toast_push(bm_app *app, const char *title, const char *desc, cui_tone tone) {
    if (!app || app->toast_count >= 8) return;
    bm_pending_toast *t = &app->toasts[app->toast_count++];
    bm_strlcpy(t->title, title ? title : "", sizeof t->title);
    bm_strlcpy(t->desc, desc ? desc : "", sizeof t->desc);
    t->tone = tone;
}

void bm_toast_flush(bm_app *app, cui_ctx *ui) {
    for (int i = 0; i < app->toast_count; i++) {
        bm_pending_toast *t = &app->toasts[i];
        cui_toast(ui, t->title, .description = t->desc[0] ? t->desc : NULL, .tone = t->tone, .duration = 4.0f);
    }
    app->toast_count = 0;
}

void bm_refresh_guard_stats_and_filters(bm_app *app, bool save_to_disk) {
    bm_guard_compute_stats(&app->store.original_guards, &app->stats);
    bm_guard_apply_filters(&app->store.original_guards, &app->filters, &app->store.guards);
    if (!app->msg_in_progress) {
        app->msg_progress_index = 0;
        app->msg_total_count = app->store.guards.count;
    }
    if (save_to_disk) {
        bm_store_save(&app->store);
    }
}

void bm_render_setting_alert(cui_ctx *ui, bm_app *app) {
    cui_card(ui, .gap = 12, .pad = 24, .width = CUI_FILL) {
        cui_row(ui, .gap = 10, .align = CUI_ALIGN_CENTER) {
            cui_icon(ui, CUI_ICON_CIRCLE_ALERT, 20, CUI_COLOR_WARNING);
            cui_heading(ui, "未找到有效的设置", 4);
        }
        cui_secondary(ui, "请前往设置页面，设置私信所用的账号以及需要管理的直播间号");
        cui_row(ui, .gap = 8) {
            if (cui_button(ui, "前往设置", .icon = CUI_ICON_SETTINGS, .variant = CUI_BUTTON_PRIMARY)) {
                app->page = BM_PAGE_SETTING;
            }
        }
    }
}
