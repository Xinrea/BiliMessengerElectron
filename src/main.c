#include "ui/app.h"
#include "core/http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void render_title_bar(cui_ctx *ui, bm_app *app) {
    bool has_traffic_lights = bm_window_has_native_traffic_lights();
    float left_pad = has_traffic_lights ? 76.0f : 12.0f;

    cui_row(ui, .id = "bm_titlebar_row", .width = CUI_FILL, .height = CUI_PX(38),
            .align = CUI_ALIGN_CENTER, .bg = CUI_COLOR_SURFACE_MUTED, .pad_y = 3) {

        /* Left: Native traffic lights spacing + App brand */
        cui_row(ui, .align = CUI_ALIGN_CENTER, .gap = 7, .pad_left = left_pad, .pad_right = 12) {
            cui_icon(ui, CUI_ICON_SEND, 15, CUI_COLOR_ACCENT_STRONG);
            cui_text(ui, "BiliMessenger", .font = CUI_FONT_MEDIUM, .size = 13.5f, .nowrap = true);
            cui_badge(ui, "v" BM_VERSION, .tone = CUI_TONE_NEUTRAL);
        }

        /* Center: Dedicated Draggable Region with current section indicator */
        cui_row(ui, .id = "bm_titlebar_drag", .width = CUI_FILL, .height = CUI_FILL,
                .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER, .gap = 6) {
            static const char *page_titles[BM_PAGE_COUNT] = {
                "数据总览", "目标列表工作台", "私信模板工作坊",
                "兑换码物料库", "私信群发任务", "系统与账号设置", "关于 BiliMessenger"
            };
            static const char *page_icons[BM_PAGE_COUNT] = {
                CUI_ICON_LAYOUT_DASHBOARD, CUI_ICON_USERS, CUI_ICON_FILE_TEXT,
                CUI_ICON_GIFT, CUI_ICON_SEND, CUI_ICON_SETTINGS, CUI_ICON_INFO
            };
            int p = (app->page >= 0 && app->page < BM_PAGE_COUNT) ? (int)app->page : 0;
            cui_icon(ui, page_icons[p], 13, CUI_COLOR_TEXT_MUTED);
            cui_text(ui, page_titles[p], .font = CUI_FONT_BODY, .size = 12.0f, .color = CUI_COLOR_TEXT_MUTED, .nowrap = true);
        }

        /* Title bar drag interaction */
        cui_id drag_id = cui_make_id(ui, "bm_titlebar_drag");
        cui_rect dr = cui_get_rect(ui, drag_id);
        cui_vec2 mp = cui_mouse_pos(ui);
        bm_window_set_drag_rect(app->native_window, dr.x, dr.y, dr.w, dr.h);
        /* Only on the press frame (Windows); macOS drags from its own mouse-down monitor */
        if (dr.w > 0 && mp.x >= dr.x && mp.x < dr.x + dr.w && mp.y >= dr.y && mp.y < dr.y + dr.h) {
            if (cui_mouse_clicked(ui, CUI_MOUSE_LEFT)) {
                bm_window_drag(app->native_window);
            }
        }

        /* Right: Global Actions & Window Controls */
        cui_row(ui, .align = CUI_ALIGN_CENTER, .gap = 6, .pad_right = 10) {
            if (app->store.room_id[0]) {
                char room_badge[64];
                snprintf(room_badge, sizeof room_badge, "房间 %s", app->store.room_id);
                if (cui_button(ui, room_badge, .icon = CUI_ICON_RADIO, .variant = CUI_BUTTON_GHOST,
                               .size = CUI_SIZE_SM, .tooltip = "当前绑定直播间（点击前往数据总览）")) {
                    app->page = BM_PAGE_SUMMARY;
                }
            }

            if (cui_button(ui, "命令", .icon = CUI_ICON_SEARCH, .variant = CUI_BUTTON_GHOST,
                           .size = CUI_SIZE_SM, .shortcut = "Cmd K", .tooltip = "打开全局快捷命令菜单 (Cmd+K)")) {
                app->cmd_palette_open = true;
            }

            if (cui_theme_toggle(ui)) {
                app->store.dark_mode = (cui_get_theme_mode(ui) == CUI_THEME_DARK);
                bm_store_save(&app->store);
            }

            if (!has_traffic_lights) {
                cui_separator(ui);
                if (cui_icon_button(ui, CUI_ICON_MINUS, .tooltip = "最小化")) {
                    bm_window_minimize(app->native_window);
                }
                bool maxed = bm_window_is_maximized(app->native_window);
                if (cui_icon_button(ui, maxed ? CUI_ICON_MINIMIZE_2 : CUI_ICON_MAXIMIZE_2, .tooltip = maxed ? "还原" : "最大化")) {
                    bm_window_maximize_toggle(app->native_window);
                }
                if (cui_icon_button(ui, CUI_ICON_X, .tooltip = "关闭")) {
                    bm_window_close(app->native_window);
                }
            }
        }
    }
}

static void render_sidebar(cui_ctx *ui, bm_app *app) {
    char guard_badge[32] = { 0 };
    char tpl_badge[32] = { 0 };
    char code_badge[32] = { 0 };
    char msg_badge[32] = { 0 };

    if (app->store.guards.count > 0) {
        snprintf(guard_badge, sizeof guard_badge, "%d", app->store.guards.count);
    }
    if (app->store.template_count > 0) {
        snprintf(tpl_badge, sizeof tpl_badge, "%d", app->store.template_count);
    }
    if (app->store.code_count > 0) {
        snprintf(code_badge, sizeof code_badge, "%d", app->store.code_count);
    }
    if (app->msg_in_progress) {
        snprintf(msg_badge, sizeof msg_badge, "%d/%d", app->msg_progress_index, app->msg_total_count);
    } else if (app->sent_msg_count > 0) {
        snprintf(msg_badge, sizeof msg_badge, "%d", app->sent_msg_count);
    }

    float width = app->sidebar_mini ? 68.0f : 236.0f;
    cui_column(ui, .width = CUI_PX(width), .height = CUI_FILL, .bg = CUI_COLOR_SURFACE_MUTED, .pad = 10, .gap = 6) {
        if (!app->sidebar_mini) {
            /* Sidebar header: Section label + collapse button */
            cui_row(ui, .pad_x = 8, .pad_y = 4, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                cui_icon(ui, CUI_ICON_MENU, 15, CUI_COLOR_TEXT_MUTED);
                cui_gap(ui, 6);
                cui_text(ui, "工作台导航", .font = CUI_FONT_MEDIUM, .size = 13, .nowrap = true);
                cui_spacer(ui);
                if (cui_icon_button(ui, CUI_ICON_PANEL_LEFT, .tooltip = "收起侧边栏")) {
                    app->sidebar_mini = true;
                }
            }

            /* Compact Account Status Card */
            const char *uname = app->account_profile.uname;
            const char *uface = app->account_profile.face;
            if (app->store.login.has_login) {
                if (cui_card_begin(ui, &(cui_card_opts){ .width = CUI_FILL, .pad = 10, .gap = 8, .dir = CUI_ROW, .align = CUI_ALIGN_CENTER, .hoverable = true })) {
                    app->page = BM_PAGE_SETTING;
                }
                cui_texture ftex = bm_image_get(ui, uface, NULL, NULL);
                cui_avatar(ui, uname[0] ? uname : "B", .size = 32, .image = ftex, .status = "online");
                cui_column(ui, .width = CUI_FILL, .gap = 1) {
                    cui_text(ui, uname[0] ? uname : "已登录账号", .font = CUI_FONT_MEDIUM, .size = 13, .nowrap = true);
                    char sub[64];
                    if (app->store.room_id[0]) {
                        snprintf(sub, sizeof sub, "房间 %s", app->store.room_id);
                    } else {
                        snprintf(sub, sizeof sub, "UID %s", app->store.login.DedeUserID);
                    }
                    cui_muted(ui, sub);
                }
                cui_icon(ui, CUI_ICON_CHEVRON_RIGHT, 14, CUI_COLOR_TEXT_MUTED);
                cui_card_end(ui);
            } else {
                if (cui_card_begin(ui, &(cui_card_opts){ .width = CUI_FILL, .pad = 10, .gap = 8, .dir = CUI_ROW, .align = CUI_ALIGN_CENTER, .hoverable = true })) {
                    app->page = BM_PAGE_SETTING;
                }
                cui_icon(ui, CUI_ICON_USER_PLUS, 18, CUI_COLOR_WARNING);
                cui_column(ui, .width = CUI_FILL, .gap = 1) {
                    cui_text(ui, "未登录账号", .font = CUI_FONT_MEDIUM, .size = 13);
                    cui_muted(ui, "点击前往扫码或配置");
                }
                cui_card_end(ui);
            }

            cui_separator(ui);

            cui_nav_section(ui, "数据与名单");
            if (cui_nav_item(ui, "数据总览", .icon = CUI_ICON_LAYOUT_DASHBOARD, .selected = app->page == BM_PAGE_SUMMARY, .group = "main_nav")) {
                app->page = BM_PAGE_SUMMARY;
            }
            if (cui_nav_item(ui, "目标列表", .icon = CUI_ICON_USERS, .selected = app->page == BM_PAGE_GUARD_LIST,
                             .badge = guard_badge[0] ? guard_badge : NULL, .group = "main_nav")) {
                app->page = BM_PAGE_GUARD_LIST;
            }

            cui_nav_section(ui, "内容与物料");
            if (cui_nav_item(ui, "模板管理", .icon = CUI_ICON_FILE_TEXT, .selected = app->page == BM_PAGE_TEMPLATE,
                             .badge = tpl_badge[0] ? tpl_badge : NULL, .group = "main_nav")) {
                app->page = BM_PAGE_TEMPLATE;
            }
            if (cui_nav_item(ui, "兑换码库", .icon = CUI_ICON_GIFT, .selected = app->page == BM_PAGE_CODES,
                             .badge = code_badge[0] ? code_badge : NULL, .group = "main_nav")) {
                app->page = BM_PAGE_CODES;
            }

            cui_nav_section(ui, "任务与系统");
            if (cui_nav_item(ui, "私信群发", .icon = CUI_ICON_SEND, .selected = app->page == BM_PAGE_MESSAGE,
                             .badge = msg_badge[0] ? msg_badge : NULL, .group = "main_nav")) {
                app->page = BM_PAGE_MESSAGE;
            }
            if (cui_nav_item(ui, "系统设置", .icon = CUI_ICON_SETTINGS, .selected = app->page == BM_PAGE_SETTING, .group = "main_nav")) {
                app->page = BM_PAGE_SETTING;
            }
            if (cui_nav_item(ui, "关于软件", .icon = CUI_ICON_INFO, .selected = app->page == BM_PAGE_ABOUT, .group = "main_nav")) {
                app->page = BM_PAGE_ABOUT;
            }

            /* Theme switch and command palette live in the title bar */
        } else {
            cui_row(ui, .pad_y = 6, .justify = CUI_JUSTIFY_CENTER, .width = CUI_FILL) {
                if (cui_icon_button(ui, CUI_ICON_PANEL_LEFT, .tooltip = "展开侧边栏")) {
                    app->sidebar_mini = false;
                }
            }
            cui_separator(ui);
            static const char *labels[BM_PAGE_COUNT] = {
                "数据总览", "目标列表", "模板管理", "兑换码库", "私信群发", "系统设置", "关于软件"
            };
            static const char *icons[BM_PAGE_COUNT] = {
                CUI_ICON_LAYOUT_DASHBOARD, CUI_ICON_USERS, CUI_ICON_FILE_TEXT,
                CUI_ICON_GIFT, CUI_ICON_SEND, CUI_ICON_SETTINGS, CUI_ICON_INFO
            };
            for (int i = 0; i < BM_PAGE_COUNT; i++) {
                cui_push_id_int(ui, i);
                cui_row(ui, .justify = CUI_JUSTIFY_CENTER, .width = CUI_FILL) {
                    if (cui_icon_button(ui, icons[i], .tooltip = labels[i], .selected = (int)app->page == i)) {
                        app->page = (bm_page_id)i;
                    }
                }
                cui_pop_id(ui);
            }
        }
    }
}

static void render_status_bar(cui_ctx *ui, bm_app *app) {
    cui_box(ui, .width = CUI_FILL, .bg = CUI_COLOR_SURFACE_MUTED) {
        cui_status_bar(ui, "app_status_bar") {
            /* Login indicator */
            if (app->store.login.has_login) {
                const char *uname = app->account_profile.uname;
                char login_txt[160];
                snprintf(login_txt, sizeof login_txt, "账号：%s", uname[0] ? uname : app->store.login.DedeUserID);
                if (cui_status_bar_item(ui, login_txt, .icon = CUI_ICON_CIRCLE_CHECK, .tone = CUI_TONE_SUCCESS, .tooltip = "点击前往账号设置")) {
                    app->page = BM_PAGE_SETTING;
                }
            } else {
                if (cui_status_bar_item(ui, "未登录账号", .icon = CUI_ICON_CIRCLE_ALERT, .tone = CUI_TONE_WARNING, .tooltip = "点击前往登录")) {
                    app->page = BM_PAGE_SETTING;
                }
            }

            /* Room indicator */
            if (app->store.room_id[0]) {
                char room_txt[160];
                if (app->room_info.anchor_uname[0]) {
                    snprintf(room_txt, sizeof room_txt, "直播间：%s (%s)", app->store.room_id, app->room_info.anchor_uname);
                } else {
                    snprintf(room_txt, sizeof room_txt, "直播间：%s", app->store.room_id);
                }
                if (cui_status_bar_item(ui, room_txt, .icon = CUI_ICON_RADIO,
                                        .tone = app->room_info.live_status == 1 ? CUI_TONE_WARNING : CUI_TONE_NEUTRAL)) {
                    app->page = BM_PAGE_SUMMARY;
                }
            } else {
                if (cui_status_bar_item(ui, "未绑定直播间", .icon = CUI_ICON_CIRCLE_ALERT, .tone = CUI_TONE_WARNING)) {
                    app->page = BM_PAGE_SETTING;
                }
            }

            /* Active background job or message sending progress */
            if (app->msg_in_progress) {
                float frac = app->msg_total_count > 0 ? (float)app->msg_progress_index / (float)app->msg_total_count : 0.0f;
                char lbl[96];
                snprintf(lbl, sizeof lbl, "%s %d/%d",
                         app->msg_is_recalling ? "正在撤回私信" : "正在群发私信",
                         app->msg_progress_index, app->msg_total_count);
                cui_status_bar_progress(ui, frac, .label = lbl, .width = 140);
            } else if (bm_jobs_active_count() > 0) {
                cui_status_bar_progress(ui, 0.0f, .label = "后台同步数据中...", .indeterminate = true, .width = 110);
            }

            cui_status_bar_spacer(ui);

            char target_txt[64], code_txt[64], interval_txt[64];
            snprintf(target_txt, sizeof target_txt, "目标 %d 人", app->store.guards.count);
            snprintf(code_txt, sizeof code_txt, "兑换码 %d", app->store.code_count);
            snprintf(interval_txt, sizeof interval_txt, "间隔 %d ms", app->store.send_interval);

            if (cui_status_bar_item(ui, target_txt, .icon = CUI_ICON_USERS)) {
                app->page = BM_PAGE_GUARD_LIST;
            }
            if (cui_status_bar_item(ui, code_txt, .icon = CUI_ICON_GIFT)) {
                app->page = BM_PAGE_CODES;
            }
            if (cui_status_bar_item(ui, interval_txt, .icon = CUI_ICON_CLOCK)) {
                app->page = BM_PAGE_SETTING;
            }
            cui_status_bar_text(ui, "v" BM_VERSION);
        }
    }
}

static void render_command_palette(cui_ctx *ui, bm_app *app) {
    if (!cui_wants_keyboard(ui) && cui_shortcut(ui, CUI_MOD_PRIMARY, CUI_KEY_K)) {
        app->cmd_palette_open = !app->cmd_palette_open;
    }

    static const cui_command cmds[] = {
        { "前往：数据总览",         CUI_ICON_LAYOUT_DASHBOARD, "页面导航", "1" },
        { "前往：目标列表",         CUI_ICON_USERS,            "页面导航", "2" },
        { "前往：模板管理",         CUI_ICON_FILE_TEXT,        "页面导航", "3" },
        { "前往：兑换码库",         CUI_ICON_GIFT,             "页面导航", "4" },
        { "前往：私信群发",         CUI_ICON_SEND,             "页面导航", "5" },
        { "前往：系统设置",         CUI_ICON_SETTINGS,         "页面导航", "6" },
        { "前往：关于软件",         CUI_ICON_INFO,             "页面导航", "7" },
        { "动作：刷新直播间与趋势数据", CUI_ICON_REFRESH_CW,   "快捷动作", NULL },
        { "动作：拉取今日实时舰长快照", CUI_ICON_CALENDAR,     "快捷动作", NULL },
        { "动作：目标列表一键去重",     CUI_ICON_FILTER,       "快捷动作", NULL },
        { "动作：切换深色 / 浅色主题",  CUI_ICON_SUN,          "界面外观", NULL }
    };
    int chosen = cui_command_palette(ui, &app->cmd_palette_open, cmds, (int)(sizeof cmds / sizeof cmds[0]));
    if (chosen >= 0 && chosen <= 6) {
        app->page = (bm_page_id)chosen;
    } else if (chosen == 7) {
        bm_summary_refresh(app);
    } else if (chosen == 8) {
        int y, m, d;
        bm_date_today(&y, &m, &d);
        char dstr[16];
        snprintf(dstr, sizeof dstr, "%04d-%02d-%02d", y, m, d);
        bm_guard_list_fetch_snapshot(app, dstr);
        app->page = BM_PAGE_GUARD_LIST;
    } else if (chosen == 9) {
        int removed = bm_guard_deduplicate(&app->store.original_guards);
        bm_refresh_guard_stats_and_filters(app, true);
        char msg[64];
        snprintf(msg, sizeof msg, "去重完成，移除 %d 条重复记录", removed);
        bm_toast_push(app, msg, NULL, CUI_TONE_SUCCESS);
    } else if (chosen == 10) {
        app->store.dark_mode = !app->store.dark_mode;
        cui_set_theme(ui, app->store.dark_mode ? CUI_THEME_DARK : CUI_THEME_LIGHT);
        bm_store_save(&app->store);
    }
}

static void render_update_dialog(cui_ctx *ui, bm_app *app) {
    char title[128];
    snprintf(title, sizeof title, "发现新版本 %s", app->update_tag);
    cui_dialog(ui, title, &app->update_dialog, .description = "是否前往 GitHub 下载最新版本？", .width = 480) {
        if (app->update_body[0]) {
            cui_box(ui, .width = CUI_FILL, .max_height = 240, .pad = 12, .bg = CUI_COLOR_SURFACE_MUTED,
                    .radius = 8, .scroll_y = true) {
                cui_text(ui, app->update_body);
            }
        }
        cui_dialog_footer(ui) {
            if (cui_button(ui, "暂不更新")) {
                app->update_dialog = false;
            }
            if (cui_button(ui, "前往 GitHub 下载", .icon = CUI_ICON_EXTERNAL_LINK, .variant = CUI_BUTTON_PRIMARY)) {
                if (app->update_url[0]) {
                    cui_app_open_url(app->update_url);
                }
                app->update_dialog = false;
            }
        }
    }
}

static void app_frame(cui_ctx *ui, void *user) {
    bm_app *app = (bm_app *)user;

    if (!app->fonts_loaded) {
        bm_fonts_init(ui);
        app->fonts_loaded = true;
    }
    if (!app->theme_applied) {
        cui_set_theme(ui, app->store.dark_mode ? CUI_THEME_DARK : CUI_THEME_LIGHT);
        app->theme_applied = true;
    }
    bm_sync_theme_tokens(ui, app);

    bm_jobs_poll_main(app);
    bm_toast_flush(app, ui);

    if (bm_jobs_active_count() > 0 || app->msg_in_progress) {
        cui_request_wakeup(ui, 0.05f);
    }

    cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .bg = CUI_COLOR_BACKGROUND) {
        render_title_bar(ui, app);
        cui_separator(ui);

        cui_row(ui, .width = CUI_FILL, .height = CUI_FILL, .align = CUI_ALIGN_STRETCH) {
            render_sidebar(ui, app);
            cui_separator(ui);

            if (app->page == BM_PAGE_GUARD_LIST || app->page == BM_PAGE_CODES ||
                app->page == BM_PAGE_TEMPLATE || app->page == BM_PAGE_MESSAGE) {
                cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 20, .gap = 14) {
                    switch (app->page) {
                        case BM_PAGE_GUARD_LIST:
                            bm_page_guard_list(ui, app);
                            break;
                        case BM_PAGE_TEMPLATE:
                            bm_page_template(ui, app);
                            break;
                        case BM_PAGE_CODES:
                            bm_page_codes(ui, app);
                            break;
                        case BM_PAGE_MESSAGE:
                            bm_page_message(ui, app);
                            break;
                        default:
                            break;
                    }
                }
            } else {
                cui_scroll(ui, .id = "main_scroll", .width = CUI_FILL, .height = CUI_FILL, .pad = 20, .gap = 16, .fade_edges = true) {
                    switch (app->page) {
                        case BM_PAGE_SUMMARY:
                            bm_page_summary(ui, app);
                            break;
                        case BM_PAGE_SETTING:
                            bm_page_setting(ui, app);
                            break;
                        case BM_PAGE_ABOUT:
                            bm_page_about(ui, app);
                            break;
                        default:
                            break;
                    }
                }
            }
        }

        render_status_bar(ui, app);
    }

    render_command_palette(ui, app);
    render_update_dialog(ui, app);
    bm_render_feedback_dialog(ui, app);
}

int main(void) {
    bm_http_init();
    bm_jobs_init();
    bm_images_init();

    static bm_app app;
    bm_app_init(&app);
    const char *env_page = getenv("BM_PAGE");
    if (env_page && *env_page) {
        int p = atoi(env_page);
        if (p >= 0 && p < BM_PAGE_COUNT) {
            app.page = (bm_page_id)p;
        }
    }
    const char *env_theme = getenv("BM_THEME");
    if (env_theme && *env_theme) {
        app.store.dark_mode = (strcmp(env_theme, "dark") == 0);
    }
    const char *env_accent = getenv("BM_ACCENT");
    if (env_accent && *env_accent) {
        app.accent_idx = atoi(env_accent);
    }
    if (getenv("BM_IMPORT_DIALOG")) {
        app.import_guard_dialog = true;
    }
    if (getenv("BM_FEEDBACK_DIALOG")) {
        app.feedback_dialog = true;
        if (!strcmp(getenv("BM_FEEDBACK_DIALOG"), "preview") && app.feedback.include_log) {
            if (!app.feedback.log[0]) bm_feedback_capture_log(&app.feedback);
            app.feedback_log_preview = true;
        }
    }
    bm_setting_startup_checks(&app);

    cui_app_desc desc = {
        .title = "BiliMessenger",
        .width = 1280,
        .height = 760,
        .min_width = 1024,
        .min_height = 640,
        .theme = app.store.dark_mode ? CUI_THEME_DARK : CUI_THEME_LIGHT,
        .accent = CUI_ACCENT_BLUE
    };

    cui_app *capp = cui_app_create(&desc);
    if (!capp) return 1;
    app.native_window = cui_app_native_window(capp);
    bm_window_setup(app.native_window);
    app.window_initialized = true;

    while (cui_app_frame(capp)) {
        app_frame(cui_app_ui(capp), &app);
    }
    cui_app_destroy(capp);

    bm_app_shutdown(&app, NULL);
    bm_jobs_shutdown();
    bm_http_cleanup();
    return 0;
}
