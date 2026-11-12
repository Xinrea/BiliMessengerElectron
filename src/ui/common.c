#include "ui/app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bm_app_init(bm_app *app) {
    bm_diagnostics_event(BM_EVENT_APP_START, BM_LOG_INFO, 0, 0);
    memset(app, 0, sizeof *app);
    bm_store_init(&app->store);
    bm_store_load(&app->store, NULL);
    snprintf(app->feedback_path, sizeof app->feedback_path, "%s.feedback.json", app->store.path);
    app->feedback.include_log = true;
    app->feedback_load_failed = !app->store.path[0]
        || !bm_feedback_load(&app->feedback, app->feedback_path);

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
    bm_diagnostics_event(BM_EVENT_APP_STOP, BM_LOG_INFO, 0, 0);
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
    /* The live queue is the broadcast cursor. Rebuilding it from the roster
       while a pass is running puts already-attempted recipients back in front
       of the in-flight send. A stopped pass rebuilds so failures, and roster
       members who were filtered out of this pass, are still there. */
    if (!app->msg_in_progress) {
        bm_guard_apply_filters(&app->store.original_guards, &app->filters, &app->store.guards);
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

void bm_guard_tier_colors(cui_ctx *ui, int guard_level, cui_color *bg, cui_color *border, cui_color *fg) {
    bool dark = (cui_get_theme_mode(ui) == CUI_THEME_DARK);
    if (guard_level == 1) {
        /* 总督 (Governor): Bilibili official #b72e2e red-gold palette */
        if (bg)     *bg     = dark ? CUI_RGBA(0xE12D392E) : CUI_RGB(0xFFF1F0);
        if (border) *border = dark ? CUI_RGBA(0xFF6B6B73) : CUI_RGB(0xFFA39E);
        if (fg)     *fg     = dark ? CUI_RGB(0xFF8A8A)    : CUI_RGB(0xB72E2E);
    } else if (guard_level == 2) {
        /* 提督 (Admiral): Bilibili official #6f3ec0 royal purple palette */
        if (bg)     *bg     = dark ? CUI_RGBA(0x9333EA2E) : CUI_RGB(0xF6EEFF);
        if (border) *border = dark ? CUI_RGBA(0xB784F773) : CUI_RGB(0xD3ADF7);
        if (fg)     *fg     = dark ? CUI_RGB(0xD8B4FE)    : CUI_RGB(0x6F3EC0);
    } else if (guard_level == 3) {
        /* 舰长 (Captain): Bilibili official ocean blue palette */
        if (bg)     *bg     = dark ? CUI_RGBA(0x1677FF2E) : CUI_RGB(0xEBF4FF);
        if (border) *border = dark ? CUI_RGBA(0x529BFF73) : CUI_RGB(0x91CAFF);
        if (fg)     *fg     = dark ? CUI_RGB(0x7CC0FF)    : CUI_RGB(0x1D5BD6);
    } else {
        /* Custom user title */
        if (bg)     *bg     = dark ? CUI_RGBA(0x14B8A629) : CUI_RGB(0xE6FFFB);
        if (border) *border = dark ? CUI_RGBA(0x2DD4BF66) : CUI_RGB(0x87E8DE);
        if (fg)     *fg     = dark ? CUI_RGB(0x5EEAD4)    : CUI_RGB(0x0D9488);
    }
}

void bm_guard_badge(cui_ctx *ui, const bm_guard *g) {
    if (!g) return;
    const char *lvl_txt = bm_guard_level_str(g);
    if (g->guard_level < 1 || g->guard_level > 3) {
        cui_badge(ui, lvl_txt, .tone = CUI_TONE_TEAL);
        return;
    }

    cui_color bg, border, fg;
    bm_guard_tier_colors(ui, g->guard_level, &bg, &border, &fg);
    cui_texture gtex = bm_guard_icon_get(ui, g->guard_level);

    cui_row(ui, .align = CUI_ALIGN_CENTER, .gap = 4,
            .pad_left = 4, .pad_right = 7, .pad_y = 2,
            .radius = 999, .bg = bg, .border = 1.0f, .border_color = border) {
        if (gtex) {
            cui_image(ui, gtex, .width = CUI_PX(16), .height = CUI_PX(16));
        }
        cui_text(ui, lvl_txt, .font = CUI_FONT_MEDIUM, .size = 11.5f, .color = fg, .nowrap = true);
    }
}

bool bm_guard_filter_chip(cui_ctx *ui, const char *id, int guard_level, const char *label, bool *active) {
    bool is_on = active ? *active : true;
    cui_id cid = cui_make_id(ui, id);
    cui_rect r = cui_get_rect(ui, cid);
    cui_vec2 mp = cui_mouse_pos(ui);
    bool hovered = (r.w > 0 && mp.x >= r.x && mp.x < r.x + r.w && mp.y >= r.y && mp.y < r.y + r.h);
    bool clicked = hovered && cui_mouse_clicked(ui, CUI_MOUSE_LEFT);
    if (clicked && active) {
        *active = !*active;
        is_on = *active;
    }

    cui_color bg, border, fg;
    if (is_on) {
        bm_guard_tier_colors(ui, guard_level, &bg, &border, &fg);
    } else {
        bg = CUI_COLOR_SURFACE_MUTED;
        border = CUI_COLOR_BORDER;
        fg = CUI_COLOR_TEXT_SECONDARY;
    }

    cui_texture gtex = bm_guard_icon_get(ui, guard_level);
    cui_row(ui, .id = id, .align = CUI_ALIGN_CENTER, .gap = 5,
            .height = CUI_PX(30), .pad_left = 8, .pad_right = 11,
            .radius = 15, .bg = bg, .border = hovered ? 1.5f : 1.0f, .border_color = border,
            .opacity = is_on ? 1.0f : 0.65f) {
        if (gtex) {
            cui_image(ui, gtex, .width = CUI_PX(18), .height = CUI_PX(18));
        }
        cui_text(ui, label, .font = CUI_FONT_MEDIUM, .size = 12.5f, .color = fg, .nowrap = true);
        cui_icon(ui, is_on ? CUI_ICON_CHECK : CUI_ICON_X, 12, fg);
    }
    return clicked;
}
