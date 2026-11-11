#include "ui/app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char room_id[64];
    bm_room_info rinfo;
    bm_chart_history guard_hist;
    bm_chart_history follower_hist;
    bool ok;
    bm_bili_err err;
} summary_job_data;

static void on_summary_loaded(void *raw, void *user) {
    summary_job_data *d = (summary_job_data *)raw;
    bm_app *app = (bm_app *)user;
    app->summary_loading = false;
    if (d->ok && strcmp(d->room_id, app->store.room_id) == 0) {
        app->room_info = d->rinfo;
        app->guard_history = d->guard_hist;
        app->follower_history = d->follower_hist;
        app->summary_loaded = true;
    } else if (!d->ok) {
        bm_toast_push(app, "获取直播间信息失败", d->err.message, CUI_TONE_DANGER);
    }
    free(d);
}

static void summary_worker(void *raw) {
    summary_job_data *d = (summary_job_data *)raw;
    if (bm_bili_get_room_info(d->room_id, &d->rinfo, &d->err)) {
        d->ok = true;
        if (d->rinfo.uid != 0) {
            bm_bili_get_vtbs_history(d->rinfo.uid, true, &d->guard_hist, NULL);
            bm_bili_get_vtbs_history(d->rinfo.uid, false, &d->follower_hist, NULL);
        }
    }
    bm_job_post_main(on_summary_loaded, d);
}

void bm_summary_refresh(bm_app *app) {
    if (!app || !app->store.room_id[0] || app->summary_loading) return;
    summary_job_data *d = (summary_job_data *)calloc(1, sizeof *d);
    if (!d) return;
    bm_strlcpy(d->room_id, app->store.room_id, sizeof d->room_id);
    app->summary_loading = true;
    if (!bm_job_spawn(summary_worker, d)) {
        app->summary_loading = false;
        free(d);
    }
}

void bm_page_summary(cui_ctx *ui, bm_app *app) {
    if (!app->store.room_id[0]) {
        bm_render_setting_alert(ui, app);
        return;
    }

    if (!app->summary_loaded && !app->summary_loading) {
        bm_summary_refresh(app);
    }

    cui_row(ui, .gap = 16, .width = CUI_FILL, .align = CUI_ALIGN_STRETCH, .wrap = true) {
        /* Left: Room Info Card */
        cui_card(ui, .width = CUI_GROW(5), .min_width = 300, .gap = 12) {
            int cw = 0, ch = 0;
            cui_texture cover_tex = bm_image_get(ui, app->room_info.cover, &cw, &ch);
            if (cover_tex) {
                cui_image(ui, cover_tex, .width = CUI_FILL, .height = CUI_PX(170), .radius = 10);
            } else {
                cui_box(ui, .width = CUI_FILL, .height = CUI_PX(170), .radius = 10, .bg = CUI_COLOR_SURFACE_MUTED,
                        .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER) {
                    if (app->summary_loading) {
                        cui_spinner(ui, 24);
                    } else {
                        cui_icon(ui, CUI_ICON_VIDEO, 32, CUI_COLOR_TEXT_MUTED);
                    }
                }
            }

            cui_row(ui, .gap = 8, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                if (app->room_info.live_status == 1) {
                    char badge_txt[64];
                    snprintf(badge_txt, sizeof badge_txt, "直播中 · 人气值：%d", app->room_info.online);
                    cui_badge(ui, badge_txt, .tone = CUI_TONE_WARNING, .dot = true);
                } else {
                    cui_badge(ui, "未开播", .tone = CUI_TONE_NEUTRAL, .dot = true);
                }
                cui_spacer(ui);
                if (cui_button(ui, "进入直播间", .icon = CUI_ICON_PLAY, .size = CUI_SIZE_SM, .variant = CUI_BUTTON_PRIMARY)) {
                    char url[256];
                    snprintf(url, sizeof url, "https://live.bilibili.com/%s", app->store.room_id);
                    cui_app_open_url(url);
                }
            }

            cui_text(ui, app->room_info.title[0] ? app->room_info.title : "加载直播间信息中...", .font = CUI_FONT_MEDIUM, .size = 15);
            cui_separator(ui);

            cui_row(ui, .gap = 10, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                cui_texture face_tex = bm_image_get(ui, app->room_info.anchor_face, NULL, NULL);
                cui_avatar(ui, app->room_info.anchor_uname[0] ? app->room_info.anchor_uname : "UP", .size = 38, .image = face_tex);
                if (cui_button(ui, app->room_info.anchor_uname[0] ? app->room_info.anchor_uname : "主播空间",
                               .variant = CUI_BUTTON_GHOST, .size = CUI_SIZE_SM, .icon_right = CUI_ICON_EXTERNAL_LINK)) {
                    if (app->room_info.uid != 0) {
                        char url[256];
                        snprintf(url, sizeof url, "https://space.bilibili.com/%llu", (unsigned long long)app->room_info.uid);
                        cui_app_open_url(url);
                    }
                }
                cui_spacer(ui);
                char att_txt[64];
                snprintf(att_txt, sizeof att_txt, "关注 %d", app->room_info.attention);
                cui_badge(ui, att_txt, .tone = CUI_TONE_WARNING, .solid = true);
            }
        }

        /* Right: Guard History Chart */
        cui_card(ui, .width = CUI_GROW(7), .min_width = 380, .gap = 12) {
            cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                cui_label(ui, "舰长数趋势（近 60 天）");
                cui_spacer(ui);
                if (app->guard_history.count > 0) {
                    char latest[64];
                    snprintf(latest, sizeof latest, "最新：%.0f", app->guard_history.values[app->guard_history.count - 1]);
                    cui_badge(ui, latest, .tone = CUI_TONE_WARNING);
                }
            }
            if (app->guard_history.count >= 2) {
                const char *labels[BM_MAX_HISTORY_POINTS];
                for (int i = 0; i < app->guard_history.count; i++) {
                    labels[i] = app->guard_history.labels[i];
                }
                cui_series s = {
                    .name = "舰长数",
                    .values = app->guard_history.values,
                    .count = app->guard_history.count,
                    .color = CUI_RGB(0xF59E0B)
                };
                cui_line_chart(ui, "guard_chart", &s, 1, .labels = labels, .label_count = app->guard_history.count,
                               .height = 215, .area = true, .smooth = true, .format = "%.0f");
            } else {
                cui_box(ui, .width = CUI_FILL, .height = CUI_PX(215), .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER) {
                    cui_muted(ui, app->summary_loading ? "正在加载舰长历史曲线..." : "暂无 vtbs.moe 舰长历史数据");
                }
            }
        }
    }

    /* Bottom: Follower History Chart */
    cui_card(ui, .width = CUI_FILL, .gap = 10) {
        cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
            cui_label(ui, "粉丝数趋势（近 60 天）");
            cui_spacer(ui);
            if (app->follower_history.count > 0) {
                char latest[64];
                snprintf(latest, sizeof latest, "最新：%.0f", app->follower_history.values[app->follower_history.count - 1]);
                cui_badge(ui, latest, .tone = CUI_TONE_WARNING);
            }
            if (cui_button(ui, "刷新数据", .icon = CUI_ICON_REFRESH_CW, .size = CUI_SIZE_SM, .loading = app->summary_loading)) {
                bm_summary_refresh(app);
            }
        }
        if (app->follower_history.count >= 2) {
            const char *labels[BM_MAX_HISTORY_POINTS];
            for (int i = 0; i < app->follower_history.count; i++) {
                labels[i] = app->follower_history.labels[i];
            }
            cui_series s = {
                .name = "粉丝数",
                .values = app->follower_history.values,
                .count = app->follower_history.count,
                .color = CUI_RGB(0xF59E0B)
            };
            cui_line_chart(ui, "follower_chart", &s, 1, .labels = labels, .label_count = app->follower_history.count,
                           .height = 195, .area = true, .smooth = true, .format = "%.0f");
        } else {
            cui_box(ui, .width = CUI_FILL, .height = CUI_PX(195), .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER) {
                cui_muted(ui, app->summary_loading ? "正在加载粉丝历史曲线..." : "暂无 vtbs.moe 粉丝历史数据");
            }
        }
        cui_row(ui, .width = CUI_FILL) {
            cui_spacer(ui);
            cui_muted(ui, "*图表数据来源自 vtbs.moe");
        }
    }
}
