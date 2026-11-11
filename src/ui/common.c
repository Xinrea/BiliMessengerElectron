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

void bm_sync_theme_tokens(cui_ctx *ui, bm_app *app) {
    static const cui_accent accents[4] = {
        CUI_ACCENT_BLUE, CUI_ACCENT_AMBER, CUI_ACCENT_ROSE, CUI_ACCENT_VIOLET
    };
    int idx = (app && app->accent_idx >= 0 && app->accent_idx < 4) ? app->accent_idx : 0;
    cui_set_accent(ui, accents[idx]);

    cui_theme *th = cui_theme_get(ui);
    if (!th) return;
    bool dark = (cui_get_theme_mode(ui) == CUI_THEME_DARK);

    /* Ensure all Primary buttons use high-contrast deep fill + pure white text
     * (WCAG 2.1 >= 4.6:1 ~ 7.1:1, APCA |Lc| >= 76 ~ 88 in both Light and Dark mode),
     * and ACCENT_STRONG has >= 6.1:1 ~ 9.9:1 contrast against card surfaces. */
    switch (idx) {
        case 1: /* Amber */
            th->colors[CUI_TOKEN_ACCENT]            = CUI_RGB(dark ? 0xC25300 : 0xB45309);
            th->colors[CUI_TOKEN_ACCENT_FOREGROUND] = CUI_RGB(0xFFFFFF);
            th->colors[CUI_TOKEN_ACCENT_STRONG]     = CUI_RGB(dark ? 0xFBBF24 : 0x9A3412);
            th->colors[CUI_TOKEN_CONTROL_ON]        = CUI_RGB(dark ? 0xC25300 : 0xB45309);
            th->colors[CUI_TOKEN_CONTROL_GLYPH]     = CUI_RGB(0xFFFFFF);
            break;
        case 2: /* Rose */
            th->colors[CUI_TOKEN_ACCENT]            = CUI_RGB(dark ? 0xDB2777 : 0xD23286);
            th->colors[CUI_TOKEN_ACCENT_FOREGROUND] = CUI_RGB(0xFFFFFF);
            th->colors[CUI_TOKEN_ACCENT_STRONG]     = CUI_RGB(dark ? 0xF472B6 : 0xB2186E);
            th->colors[CUI_TOKEN_CONTROL_ON]        = CUI_RGB(dark ? 0xDB2777 : 0xD23286);
            th->colors[CUI_TOKEN_CONTROL_GLYPH]     = CUI_RGB(0xFFFFFF);
            break;
        case 3: /* Violet */
            th->colors[CUI_TOKEN_ACCENT]            = CUI_RGB(dark ? 0x7C3AED : 0x6D28D9);
            th->colors[CUI_TOKEN_ACCENT_FOREGROUND] = CUI_RGB(0xFFFFFF);
            th->colors[CUI_TOKEN_ACCENT_STRONG]     = CUI_RGB(dark ? 0xA78BFA : 0x5B21B6);
            th->colors[CUI_TOKEN_CONTROL_ON]        = CUI_RGB(dark ? 0x7C3AED : 0x6D28D9);
            th->colors[CUI_TOKEN_CONTROL_GLYPH]     = CUI_RGB(0xFFFFFF);
            break;
        default: /* Blue (Default) */
            th->colors[CUI_TOKEN_ACCENT]            = CUI_RGB(dark ? 0x2563EB : 0x0562EF);
            th->colors[CUI_TOKEN_ACCENT_FOREGROUND] = CUI_RGB(0xFFFFFF);
            th->colors[CUI_TOKEN_ACCENT_STRONG]     = CUI_RGB(dark ? 0x60A5FA : 0x074DB7);
            th->colors[CUI_TOKEN_CONTROL_ON]        = CUI_RGB(dark ? 0x2563EB : 0x0562EF);
            th->colors[CUI_TOKEN_CONTROL_GLYPH]     = CUI_RGB(0xFFFFFF);
            break;
    }
}

void bm_render_setting_alert(cui_ctx *ui, bm_app *app) {
    cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER, .pad = 24) {
        cui_card(ui, .width = CUI_PX(540), .gap = 16, .pad = 28) {
            cui_row(ui, .gap = 12, .align = CUI_ALIGN_CENTER) {
                cui_icon(ui, CUI_ICON_SPARKLES, 24, CUI_COLOR_ACCENT_STRONG);
                cui_column(ui, .gap = 2) {
                    cui_heading(ui, "欢迎使用 BiliMessenger", 3);
                    cui_secondary(ui, "开始之前，请先完成以下基础配置");
                }
            }

            cui_check_step steps[2] = {
                {
                    .title = "1. 登录 Bilibili 账号",
                    .description = "用于拉取直播间上舰流水以及向目标舰长发送私信（支持客户端扫码或 Cookie 登录）",
                    .action = "去登录",
                    .done = app->store.login.has_login
                },
                {
                    .title = "2. 绑定目标直播间房间号",
                    .description = "输入需要管理的 B 站直播间号，用于拉取房间实况、60 天趋势图与舰长快照",
                    .action = "去设置房间号",
                    .done = app->store.room_id[0] != '\0'
                }
            };
            if (cui_checklist(ui, "onboarding_steps", steps, 2, .title = "初始化配置清单") >= 0) {
                app->page = BM_PAGE_SETTING;
            }

            cui_row(ui, .justify = CUI_JUSTIFY_END, .width = CUI_FILL) {
                if (cui_button(ui, "前往设置中心", .icon = CUI_ICON_SETTINGS, .variant = CUI_BUTTON_PRIMARY)) {
                    app->page = BM_PAGE_SETTING;
                }
            }
        }
    }
}
