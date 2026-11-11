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

static void compute_history_stats(const bm_chart_history *h, float *out_min, float *out_max, float *out_latest, float *out_delta_pct) {
    *out_min = 0;
    *out_max = 0;
    *out_latest = 0;
    *out_delta_pct = 0;
    if (!h || h->count <= 0) return;
    float mn = h->values[0], mx = h->values[0];
    for (int i = 1; i < h->count; i++) {
        if (h->values[i] < mn) mn = h->values[i];
        if (h->values[i] > mx) mx = h->values[i];
    }
    *out_min = mn;
    *out_max = mx;
    *out_latest = h->values[h->count - 1];
    if (h->values[0] > 0.0f) {
        *out_delta_pct = ((*out_latest - h->values[0]) / h->values[0]) * 100.0f;
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

    /* Top Header Bar */
    cui_row(ui, .width = CUI_FILL, .align = CUI_ALIGN_CENTER, .gap = 10, .wrap = true) {
        cui_column(ui, .gap = 2) {
            cui_row(ui, .gap = 8, .align = CUI_ALIGN_CENTER) {
                cui_heading(ui, "直播间数据总览", 3);
                if (app->room_info.live_status == 1) {
                    cui_badge(ui, "直播中", .tone = CUI_TONE_SUCCESS, .dot = true);
                } else {
                    cui_badge(ui, "未开播", .tone = CUI_TONE_NEUTRAL, .dot = true);
                }
            }
            char sub[160];
            snprintf(sub, sizeof sub, "房间号 %s · 主播 %s",
                     app->store.room_id,
                     app->room_info.anchor_uname[0] ? app->room_info.anchor_uname : "加载中...");
            cui_secondary(ui, sub);
        }
        cui_spacer(ui);
        if (cui_button(ui, "主播空间", .icon = CUI_ICON_USER, .size = CUI_SIZE_SM)) {
            if (app->room_info.uid != 0) {
                char url[256];
                snprintf(url, sizeof url, "https://space.bilibili.com/%llu", (unsigned long long)app->room_info.uid);
                cui_app_open_url(url);
            }
        }
        if (cui_button(ui, "进入直播间", .icon = CUI_ICON_EXTERNAL_LINK, .size = CUI_SIZE_SM)) {
            char url[256];
            snprintf(url, sizeof url, "https://live.bilibili.com/%s", app->store.room_id);
            cui_app_open_url(url);
        }
        if (cui_button(ui, "刷新数据", .icon = CUI_ICON_REFRESH_CW, .size = CUI_SIZE_SM,
                       .variant = CUI_BUTTON_PRIMARY, .loading = app->summary_loading)) {
            bm_summary_refresh(app);
        }
    }

    /* Row 1: 4 Core KPI Metric Cards */
    float g_min, g_max, g_latest, g_delta;
    float f_min, f_max, f_latest, f_delta;
    compute_history_stats(&app->guard_history, &g_min, &g_max, &g_latest, &g_delta);
    compute_history_stats(&app->follower_history, &f_min, &f_max, &f_latest, &f_delta);

    char kpi_guard_val[64], kpi_fans_val[64], kpi_online_val[64], kpi_ready_val[64], kpi_ready_sub[64];
    if (app->guard_history.count > 0) {
        snprintf(kpi_guard_val, sizeof kpi_guard_val, "%.0f", g_latest);
    } else {
        snprintf(kpi_guard_val, sizeof kpi_guard_val, "%d", app->stats.total);
    }

    if (app->follower_history.count > 0) {
        snprintf(kpi_fans_val, sizeof kpi_fans_val, "%.0f", f_latest);
    } else {
        snprintf(kpi_fans_val, sizeof kpi_fans_val, "%d", app->room_info.attention);
    }

    snprintf(kpi_online_val, sizeof kpi_online_val, "%d", app->room_info.online);
    snprintf(kpi_ready_val, sizeof kpi_ready_val, "%d 人", app->store.guards.count);
    snprintf(kpi_ready_sub, sizeof kpi_ready_sub, "兑换码库存 %d", app->store.code_count);

    cui_row(ui, .gap = 12, .width = CUI_FILL, .align = CUI_ALIGN_STRETCH) {
        cui_metric(ui, "实时大航海数", kpi_guard_val,
                   .icon = CUI_ICON_SHIELD,
                   .delta = g_delta,
                   .delta_label = "近 60 天",
                   .width = CUI_GROW(1));

        cui_metric(ui, "主播粉丝总数", kpi_fans_val,
                   .icon = CUI_ICON_HEART,
                   .delta = f_delta,
                   .delta_label = "近 60 天",
                   .width = CUI_GROW(1));

        cui_metric(ui, "直播间人气值", kpi_online_val,
                   .icon = CUI_ICON_ACTIVITY,
                   .delta_label = app->room_info.live_status == 1 ? "当前正在直播" : "当前未开播",
                   .width = CUI_GROW(1));

        cui_metric(ui, "待发目标名单", kpi_ready_val,
                   .icon = CUI_ICON_SEND,
                   .delta_label = kpi_ready_sub,
                   .width = CUI_GROW(1));
    }

    /* Row 2: Left Room Profile & Quick Actions + Right Trend Charts */
    cui_row(ui, .gap = 16, .width = CUI_FILL, .align = CUI_ALIGN_STRETCH) {
        /* Left Column: Room Live Card & Composition */
        cui_card(ui, .width = CUI_PX(340), .gap = 12, .pad = 16) {
            int cw = 0, ch = 0;
            cui_texture cover_tex = bm_image_get(ui, app->room_info.cover, &cw, &ch);
            float disp_w = 340.0f - 32.0f;
            float disp_h = (cw > 0 && ch > 0) ? (disp_w * (float)ch / (float)cw) : (disp_w * 9.0f / 16.0f);
            if (cover_tex) {
                cui_image(ui, cover_tex, .width = CUI_PX(disp_w), .height = CUI_PX(disp_h), .radius = 10);
            } else {
                cui_box(ui, .width = CUI_PX(disp_w), .height = CUI_PX(disp_h), .radius = 10, .bg = CUI_COLOR_SURFACE_MUTED,
                        .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER) {
                    if (app->summary_loading) {
                        cui_spinner(ui, 24);
                    } else {
                        cui_icon(ui, CUI_ICON_VIDEO, 32, CUI_COLOR_TEXT_MUTED);
                    }
                }
            }

            cui_row(ui, .gap = 10, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                cui_texture face_tex = bm_image_get(ui, app->room_info.anchor_face, NULL, NULL);
                cui_avatar(ui, app->room_info.anchor_uname[0] ? app->room_info.anchor_uname : "UP", .size = 38, .image = face_tex);
                cui_column(ui, .width = CUI_FILL, .gap = 2) {
                    cui_text(ui, app->room_info.title[0] ? app->room_info.title : "正在获取直播间标题...",
                             .font = CUI_FONT_MEDIUM, .size = 14, .nowrap = true);
                    char uid_line[96];
                    snprintf(uid_line, sizeof uid_line, "%s · UID %llu",
                             app->room_info.anchor_uname[0] ? app->room_info.anchor_uname : "主播",
                             (unsigned long long)app->room_info.uid);
                    cui_muted(ui, uid_line);
                }
            }

            cui_separator(ui);

            cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                cui_label(ui, "当前目标名单构成");
                cui_spacer(ui);
                char cnt_str[32];
                snprintf(cnt_str, sizeof cnt_str, "共 %d 人", app->stats.total);
                cui_muted(ui, cnt_str);
            }

            if (app->stats.total > 0) {
                cui_viz_item parts[3] = {
                    { "总督", (float)app->stats.type1, CUI_RGB(0xE13D3D) },
                    { "提督", (float)app->stats.type2, CUI_RGB(0x8B44E8) },
                    { "舰长", (float)app->stats.type3, CUI_RGB(0x2B7DE9) }
                };
                cui_split_bar(ui, "guard_comp_bar", parts, 3, .height = 10, .format = "%.0f", .no_legend = true);
                cui_row(ui, .gap = 6, .wrap = true, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                    static const char *tier_names[3] = { "总督", "提督", "舰长" };
                    int tier_counts[3] = { app->stats.type1, app->stats.type2, app->stats.type3 };
                    for (int ti = 0; ti < 3; ti++) {
                        int lvl = ti + 1;
                        cui_color bg, border, fg;
                        bm_guard_tier_colors(ui, lvl, &bg, &border, &fg);
                        cui_texture gtex = bm_guard_icon_get(ui, lvl);
                        char chip_txt[48];
                        snprintf(chip_txt, sizeof chip_txt, "%s %d", tier_names[ti], tier_counts[ti]);
                        cui_row(ui, .align = CUI_ALIGN_CENTER, .gap = 4,
                                .pad_left = 5, .pad_right = 8, .pad_y = 2,
                                .radius = 999, .bg = bg, .border = 1.0f, .border_color = border) {
                            if (gtex) cui_image(ui, gtex, .width = CUI_PX(15), .height = CUI_PX(15));
                            cui_text(ui, chip_txt, .font = CUI_FONT_MEDIUM, .size = 11.5f, .color = fg, .nowrap = true);
                        }
                    }
                }
            } else {
                cui_muted(ui, "目标列表为空，可前往「目标列表」拉取快照或上舰记录");
            }

            cui_spacer(ui);
            cui_separator(ui);

            cui_row(ui, .gap = 8, .width = CUI_FILL) {
                if (cui_button(ui, "管理目标名单", .icon = CUI_ICON_USERS, .size = CUI_SIZE_SM, .width = CUI_GROW(1))) {
                    app->page = BM_PAGE_GUARD_LIST;
                }
                if (cui_button(ui, "前往群发私信", .icon = CUI_ICON_SEND, .size = CUI_SIZE_SM,
                               .variant = CUI_BUTTON_PRIMARY, .width = CUI_GROW(1))) {
                    app->page = BM_PAGE_MESSAGE;
                }
            }
        }

        /* Right Column: Trend Analytics Card */
        cui_card(ui, .width = CUI_FILL, .gap = 12, .pad = 16) {
            cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL, .gap = 8, .wrap = true) {
                cui_column(ui, .gap = 2) {
                    cui_heading(ui, "近 60 天历史趋势分析", 4);
                    cui_muted(ui, "数据来源：vtbs.moe 历史观测记录");
                }
                cui_spacer(ui);
                static const char *tab_items[] = { "双图对照", "舰长趋势", "粉丝趋势" };
                cui_segmented(ui, "summary_chart_tabs", tab_items, 3, &app->summary_chart_tab, .size = CUI_SIZE_SM);
            }

            cui_separator(ui);

            const char *g_labels[BM_MAX_HISTORY_POINTS];
            for (int i = 0; i < app->guard_history.count; i++) g_labels[i] = app->guard_history.labels[i];

            const char *f_labels[BM_MAX_HISTORY_POINTS];
            for (int i = 0; i < app->follower_history.count; i++) f_labels[i] = app->follower_history.labels[i];

            if (app->summary_chart_tab == 0) {
                /* Dual compact charts stacked */
                cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                    cui_label(ui, "大航海舰长数趋势");
                    cui_spacer(ui);
                    if (app->guard_history.count >= 2) {
                        char stat_txt[128];
                        snprintf(stat_txt, sizeof stat_txt, "最低 %.0f · 最高 %.0f · 最新 %.0f", g_min, g_max, g_latest);
                        cui_badge(ui, stat_txt, .tone = CUI_TONE_WARNING);
                    }
                }
                if (app->guard_history.count >= 2) {
                    cui_series s = {
                        .name = "舰长数",
                        .values = app->guard_history.values,
                        .count = app->guard_history.count,
                        .color = CUI_RGB(0xF59E0B)
                    };
                    cui_line_chart(ui, "guard_chart_dual", &s, 1, .labels = g_labels, .label_count = app->guard_history.count,
                                   .height = 140, .area = true, .smooth = true, .no_legend = true, .format = "%.0f");
                } else {
                    cui_box(ui, .width = CUI_FILL, .height = CUI_PX(140), .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER) {
                        cui_muted(ui, app->summary_loading ? "正在加载舰长历史曲线..." : "暂无 vtbs.moe 舰长历史数据");
                    }
                }

                cui_separator(ui);

                cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                    cui_label(ui, "粉丝关注数趋势");
                    cui_spacer(ui);
                    if (app->follower_history.count >= 2) {
                        char stat_txt[128];
                        snprintf(stat_txt, sizeof stat_txt, "最低 %.0f · 最高 %.0f · 最新 %.0f", f_min, f_max, f_latest);
                        cui_badge(ui, stat_txt, .tone = CUI_TONE_INFO);
                    }
                }
                if (app->follower_history.count >= 2) {
                    cui_series s = {
                        .name = "粉丝数",
                        .values = app->follower_history.values,
                        .count = app->follower_history.count,
                        .color = CUI_RGB(0x3B82F6)
                    };
                    cui_line_chart(ui, "follower_chart_dual", &s, 1, .labels = f_labels, .label_count = app->follower_history.count,
                                   .height = 140, .area = true, .smooth = true, .no_legend = true, .format = "%.0f");
                } else {
                    cui_box(ui, .width = CUI_FILL, .height = CUI_PX(140), .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER) {
                        cui_muted(ui, app->summary_loading ? "正在加载粉丝历史曲线..." : "暂无 vtbs.moe 粉丝历史数据");
                    }
                }
            } else if (app->summary_chart_tab == 1) {
                /* Single large Guard chart */
                cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL, .gap = 8) {
                    cui_label(ui, "大航海舰长数趋势（近 60 天详图）");
                    cui_spacer(ui);
                    if (app->guard_history.count >= 2) {
                        char b1[64], b2[64], b3[64];
                        snprintf(b1, sizeof b1, "区间最低：%.0f", g_min);
                        snprintf(b2, sizeof b2, "区间最高：%.0f", g_max);
                        snprintf(b3, sizeof b3, "当前最新：%.0f", g_latest);
                        cui_badge(ui, b1, .tone = CUI_TONE_NEUTRAL);
                        cui_badge(ui, b2, .tone = CUI_TONE_INFO);
                        cui_badge(ui, b3, .tone = CUI_TONE_WARNING);
                    }
                }
                if (app->guard_history.count >= 2) {
                    cui_series s = {
                        .name = "舰长数",
                        .values = app->guard_history.values,
                        .count = app->guard_history.count,
                        .color = CUI_RGB(0xF59E0B)
                    };
                    cui_line_chart(ui, "guard_chart_large", &s, 1, .labels = g_labels, .label_count = app->guard_history.count,
                                   .height = 310, .area = true, .smooth = true, .format = "%.0f");
                } else {
                    cui_box(ui, .width = CUI_FILL, .height = CUI_PX(310), .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER) {
                        cui_muted(ui, app->summary_loading ? "正在加载舰长历史曲线..." : "暂无 vtbs.moe 舰长历史数据");
                    }
                }
            } else {
                /* Single large Follower chart */
                cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL, .gap = 8) {
                    cui_label(ui, "粉丝关注数趋势（近 60 天详图）");
                    cui_spacer(ui);
                    if (app->follower_history.count >= 2) {
                        char b1[64], b2[64], b3[64];
                        snprintf(b1, sizeof b1, "区间最低：%.0f", f_min);
                        snprintf(b2, sizeof b2, "区间最高：%.0f", f_max);
                        snprintf(b3, sizeof b3, "当前最新：%.0f", f_latest);
                        cui_badge(ui, b1, .tone = CUI_TONE_NEUTRAL);
                        cui_badge(ui, b2, .tone = CUI_TONE_INFO);
                        cui_badge(ui, b3, .tone = CUI_TONE_WARNING);
                    }
                }
                if (app->follower_history.count >= 2) {
                    cui_series s = {
                        .name = "粉丝数",
                        .values = app->follower_history.values,
                        .count = app->follower_history.count,
                        .color = CUI_RGB(0x3B82F6)
                    };
                    cui_line_chart(ui, "follower_chart_large", &s, 1, .labels = f_labels, .label_count = app->follower_history.count,
                                   .height = 310, .area = true, .smooth = true, .format = "%.0f");
                } else {
                    cui_box(ui, .width = CUI_FILL, .height = CUI_PX(310), .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER) {
                        cui_muted(ui, app->summary_loading ? "正在加载粉丝历史曲线..." : "暂无 vtbs.moe 粉丝历史数据");
                    }
                }
            }
        }
    }
}
