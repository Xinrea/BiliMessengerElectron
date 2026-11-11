#include "ui/app.h"
#include "core/http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void render_sidebar(cui_ctx *ui, bm_app *app) {
    static const char *labels[BM_PAGE_COUNT] = {
        "总览",
        "目标列表",
        "模板管理",
        "兑换码管理",
        "私信群发",
        "设置",
        "关于"
    };
    static const char *icons[BM_PAGE_COUNT] = {
        CUI_ICON_HOUSE,
        CUI_ICON_LIST,
        CUI_ICON_FILE,
        CUI_ICON_GIFT,
        CUI_ICON_MESSAGE_SQUARE,
        CUI_ICON_SETTINGS,
        CUI_ICON_INFO
    };

    char guard_badge[32] = { 0 };
    char code_badge[32] = { 0 };
    if (app->store.guards.count > 0) {
        snprintf(guard_badge, sizeof guard_badge, "%d", app->store.guards.count);
    }
    if (app->store.code_count > 0) {
        snprintf(code_badge, sizeof code_badge, "%d", app->store.code_count);
    }

    float width = app->sidebar_mini ? 64.0f : 228.0f;
    cui_column(ui, .width = CUI_PX(width), .height = CUI_FILL, .bg = CUI_COLOR_SURFACE_MUTED, .pad = 10, .gap = 4) {
        if (!app->sidebar_mini) {
            cui_row(ui, .pad_x = 8, .pad_y = 6, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                cui_column(ui, .width = CUI_FILL, .gap = 2) {
                    cui_text(ui, "BiliMessenger", .font = CUI_FONT_MEDIUM, .size = 16, .nowrap = true);
                    cui_muted(ui, "舰长私信助手");
                }
                if (cui_icon_button(ui, CUI_ICON_CHEVRON_LEFT, .tooltip = "收起侧栏")) {
                    app->sidebar_mini = true;
                }
            }
        } else {
            cui_row(ui, .pad_y = 6, .justify = CUI_JUSTIFY_CENTER, .width = CUI_FILL) {
                if (cui_icon_button(ui, CUI_ICON_CHEVRON_RIGHT, .tooltip = "展开侧栏")) {
                    app->sidebar_mini = false;
                }
            }
        }

        cui_separator(ui);
        cui_gap(ui, 4);

        for (int i = 0; i < BM_PAGE_COUNT; i++) {
            const char *badge = NULL;
            if (i == BM_PAGE_GUARD_LIST && guard_badge[0]) badge = guard_badge;
            if (i == BM_PAGE_CODES && code_badge[0]) badge = code_badge;

            if (!app->sidebar_mini) {
                if (cui_nav_item(ui, labels[i], .icon = icons[i], .selected = (int)app->page == i, .badge = badge)) {
                    app->page = (bm_page_id)i;
                }
            } else {
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
            if (cui_button(ui, "否")) {
                app->update_dialog = false;
            }
            if (cui_button(ui, "是，前往下载", .icon = CUI_ICON_EXTERNAL_LINK, .variant = CUI_BUTTON_PRIMARY)) {
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
        cui_set_accent(ui, CUI_ACCENT_AMBER);
        app->theme_applied = true;
    }

    bm_jobs_poll_main(app);
    bm_toast_flush(app, ui);

    if (bm_jobs_active_count() > 0 || app->msg_in_progress) {
        cui_request_wakeup(ui, 0.05f);
    }

    cui_row(ui, .width = CUI_FILL, .height = CUI_FILL, .align = CUI_ALIGN_STRETCH, .bg = CUI_COLOR_BACKGROUND) {
        render_sidebar(ui, app);
        cui_separator(ui);

        if (app->page == BM_PAGE_GUARD_LIST || app->page == BM_PAGE_CODES) {
            cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 20, .gap = 16) {
                if (app->page == BM_PAGE_GUARD_LIST) {
                    bm_page_guard_list(ui, app);
                } else {
                    bm_page_codes(ui, app);
                }
            }
        } else {
            cui_scroll(ui, .id = "main_scroll", .width = CUI_FILL, .height = CUI_FILL, .pad = 20, .gap = 16, .fade_edges = true) {
                switch (app->page) {
                    case BM_PAGE_SUMMARY:
                        bm_page_summary(ui, app);
                        break;
                    case BM_PAGE_TEMPLATE:
                        bm_page_template(ui, app);
                        break;
                    case BM_PAGE_MESSAGE:
                        bm_page_message(ui, app);
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

    render_update_dialog(ui, app);
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
    bm_setting_startup_checks(&app);

    cui_app_desc desc = {
        .title = "BiliMessenger",
        .width = 1280,
        .height = 720,
        .min_width = 960,
        .min_height = 600,
        .theme = app.store.dark_mode ? CUI_THEME_DARK : CUI_THEME_LIGHT,
        .accent = CUI_ACCENT_AMBER
    };

    int rc = cui_app_run(&desc, app_frame, &app);

    bm_app_shutdown(&app, NULL);
    bm_jobs_shutdown();
    bm_http_cleanup();
    return rc;
}
