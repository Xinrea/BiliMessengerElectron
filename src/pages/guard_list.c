#include "ui/app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    char room_id[64];
    bm_date_list dates;
    bool ok;
} cal_job_data;

static void on_calendar_loaded(void *raw, void *user) {
    cal_job_data *d = (cal_job_data *)raw;
    bm_app *app = (bm_app *)user;
    if (strcmp(d->room_id, app->store.room_id) == 0 && d->ok) {
        bm_date_list_clear(&app->valid_dates);
        for (int i = 0; i < d->dates.count; i++) {
            bm_date_list_push(&app->valid_dates, d->dates.dates[i]);
        }
    }
    bm_date_list_free(&d->dates);
    free(d);
}

static void calendar_worker(void *raw) {
    cal_job_data *d = (cal_job_data *)raw;
    bm_date_list_init(&d->dates);
    d->ok = bm_bili_get_guard_valid_dates(d->room_id, &d->dates, NULL);
    bm_job_post_main(on_calendar_loaded, d);
}

void bm_guard_list_refresh_calendar(bm_app *app) {
    if (!app || !app->store.room_id[0]) return;
    cal_job_data *d = (cal_job_data *)calloc(1, sizeof *d);
    if (!d) return;
    bm_strlcpy(d->room_id, app->store.room_id, sizeof d->room_id);
    if (!bm_job_spawn(calendar_worker, d)) {
        free(d);
    }
}

typedef struct {
    char room_id[64];
    char date[16];
    bm_guard_list guards;
    bool ok;
    bm_bili_err err;
} snapshot_job_data;

static void on_snapshot_loaded(void *raw, void *user) {
    snapshot_job_data *d = (snapshot_job_data *)raw;
    bm_app *app = (bm_app *)user;
    app->guard_loading = false;
    if (d->ok) {
        bm_guard_list_copy(&app->store.original_guards, &d->guards);
        bm_refresh_guard_stats_and_filters(app, true);
        char msg[128];
        snprintf(msg, sizeof msg, "已加载 %s 的舰长列表（共 %d 人）", d->date, d->guards.count);
        bm_toast_push(app, "列表已更新", msg, CUI_TONE_SUCCESS);
    } else {
        bm_toast_push(app, "获取舰长列表失败", d->err.message, CUI_TONE_DANGER);
    }
    bm_guard_list_free(&d->guards);
    free(d);
}

static void snapshot_worker(void *raw) {
    snapshot_job_data *d = (snapshot_job_data *)raw;
    bm_guard_list_init(&d->guards);
    d->ok = bm_bili_get_guard_snapshot(d->room_id, d->date, &d->guards, &d->err);
    bm_job_post_main(on_snapshot_loaded, d);
}

void bm_guard_list_fetch_snapshot(bm_app *app, const char *date_str) {
    if (!app || !app->store.room_id[0] || app->guard_loading) return;
    snapshot_job_data *d = (snapshot_job_data *)calloc(1, sizeof *d);
    if (!d) return;
    bm_strlcpy(d->room_id, app->store.room_id, sizeof d->room_id);
    if (date_str && date_str[0]) {
        bm_strlcpy(d->date, date_str, sizeof d->date);
    } else {
        bm_date_today_str(d->date, sizeof d->date);
    }
    app->guard_loading = true;
    if (!bm_job_spawn(snapshot_worker, d)) {
        app->guard_loading = false;
        free(d);
    }
}

void bm_guard_list_refresh_all(bm_app *app) {
    bm_guard_list_refresh_calendar(app);
    char today[16];
    bm_date_today_str(today, sizeof today);
    bm_guard_list_fetch_snapshot(app, today);
}

typedef struct {
    bm_login_info login;
    char begin_date[16];
    char end_date[16];
    bm_guard_list guards;
    bool ok;
    bm_bili_err err;
} range_job_data;

static int cmp_guard_time_asc(const void *a, const void *b) {
    const bm_guard *ga = (const bm_guard *)a;
    const bm_guard *gb = (const bm_guard *)b;
    return strcmp(ga->time, gb->time);
}

static void on_range_loaded(void *raw, void *user) {
    range_job_data *d = (range_job_data *)raw;
    bm_app *app = (bm_app *)user;
    app->guard_loading = false;
    if (d->ok) {
        if (d->guards.count > 1) {
            qsort(d->guards.items, (size_t)d->guards.count, sizeof(bm_guard), cmp_guard_time_asc);
        }
        bm_guard_list_copy(&app->store.original_guards, &d->guards);
        bm_refresh_guard_stats_and_filters(app, true);
        char desc[128];
        snprintf(desc, sizeof desc, "%s 至 %s 共获取 %d 条上舰记录", d->begin_date, d->end_date, d->guards.count);
        bm_toast_push(app, "上舰记录已更新", desc, CUI_TONE_SUCCESS);
    } else {
        char desc[512];
        if (d->err.code == -101) {
            snprintf(desc, sizeof desc, "%s（登录状态已过期，请重新登录）", d->err.message);
        } else {
            bm_strlcpy(desc, d->err.message, sizeof desc);
        }
        bm_toast_push(app, "上舰记录获取失败", desc, CUI_TONE_DANGER);
    }
    bm_guard_list_free(&d->guards);
    free(d);
}

static void range_worker(void *raw) {
    range_job_data *d = (range_job_data *)raw;
    bm_guard_list_init(&d->guards);
    d->ok = bm_bili_get_received_guards_by_period(&d->login, d->begin_date, d->end_date, &d->guards, &d->err);
    bm_job_post_main(on_range_loaded, d);
}

static void start_fetch_gift_range(bm_app *app) {
    if (app->gift_range.start.year == 0 || app->gift_range.end.year == 0) {
        bm_toast_push(app, "请选择日期范围", "请先在日期范围选择器中选择起止日期", CUI_TONE_WARNING);
        return;
    }
    if (!app->store.login.has_login) {
        bm_toast_push(app, "请先登录", "查询上舰记录需要先在设置页登录 Bilibili 账号", CUI_TONE_WARNING);
        return;
    }
    if (app->guard_loading) return;

    range_job_data *d = (range_job_data *)calloc(1, sizeof *d);
    if (!d) return;
    d->login = app->store.login;
    snprintf(d->begin_date, sizeof d->begin_date, "%04d-%02d-%02d",
             app->gift_range.start.year, app->gift_range.start.month, app->gift_range.start.day);
    snprintf(d->end_date, sizeof d->end_date, "%04d-%02d-%02d",
             app->gift_range.end.year, app->gift_range.end.month, app->gift_range.end.day);
    if (strcmp(d->begin_date, d->end_date) > 0) {
        char tmp[16];
        bm_strlcpy(tmp, d->begin_date, sizeof tmp);
        bm_strlcpy(d->begin_date, d->end_date, sizeof d->begin_date);
        bm_strlcpy(d->end_date, tmp, sizeof d->end_date);
    }
    app->guard_loading = true;
    if (!bm_job_spawn(range_worker, d)) {
        app->guard_loading = false;
        free(d);
    }
}

typedef struct {
    bm_login_info login;
    bool by_username;
    char keyword[128];
    char title[64];
    bm_user_profile prof;
    bool ok;
    bm_bili_err err;
} add_guard_job_data;

static void on_add_guard_done(void *raw, void *user) {
    add_guard_job_data *d = (add_guard_job_data *)raw;
    bm_app *app = (bm_app *)user;
    app->guard_loading = false;
    if (!d->ok || d->prof.mid == 0) {
        bm_toast_push(app, "添加用户失败", d->err.message[0] ? d->err.message : "未找到该用户", CUI_TONE_DANGER);
        free(d);
        return;
    }
    if (bm_guard_list_find_uid(&app->store.original_guards, d->prof.mid) >= 0) {
        char msg[160];
        snprintf(msg, sizeof msg, "用户 %s 已存在", d->prof.uname);
        bm_toast_push(app, msg, NULL, CUI_TONE_WARNING);
        free(d);
        return;
    }
    bm_guard g;
    memset(&g, 0, sizeof g);
    bm_strlcpy(g.face, d->prof.face, sizeof g.face);
    bm_strlcpy(g.username, d->prof.uname, sizeof g.username);
    bm_guard_set_level_from_tag(&g, d->title);
    g.is_alive = 0;
    g.rank = 1;
    g.ruid = 61639371ULL;
    g.uid = d->prof.mid;
    bm_guard_list_unshift(&app->store.original_guards, &g);
    bm_refresh_guard_stats_and_filters(app, true);
    bm_toast_push(app, "已添加用户", g.username, CUI_TONE_SUCCESS);
    free(d);
}

static void add_guard_worker(void *raw) {
    add_guard_job_data *d = (add_guard_job_data *)raw;
    if (d->by_username) {
        d->ok = bm_bili_search_user(&d->login, d->keyword, &d->prof, &d->err);
    } else {
        uint64_t mid = (uint64_t)strtoull(d->keyword, NULL, 10);
        d->ok = (mid > 0) && bm_bili_get_user_info(&d->login, mid, &d->prof, &d->err);
    }
    bm_job_post_main(on_add_guard_done, d);
}

static void submit_add_guard(bm_app *app) {
    char kw[128];
    bm_strlcpy(kw, app->add_guard_keyword, sizeof kw);
    char *trimmed = bm_trim(kw);
    if (!trimmed[0]) {
        bm_toast_push(app, "请输入 UID 或用户名", NULL, CUI_TONE_WARNING);
        return;
    }
    if (app->add_guard_by_username && !app->store.login.has_login) {
        bm_toast_push(app, "用户名搜索请先登录", NULL, CUI_TONE_WARNING);
        return;
    }
    if (!app->add_guard_by_username) {
        uint64_t uid = (uint64_t)strtoull(trimmed, NULL, 10);
        if (uid == 0) {
            bm_toast_push(app, "UID 必须为有效数字", NULL, CUI_TONE_WARNING);
            return;
        }
        if (bm_guard_list_find_uid(&app->store.original_guards, uid) >= 0) {
            char msg[160];
            snprintf(msg, sizeof msg, "用户 %s 已存在", trimmed);
            bm_toast_push(app, msg, NULL, CUI_TONE_WARNING);
            return;
        }
    }

    add_guard_job_data *d = (add_guard_job_data *)calloc(1, sizeof *d);
    if (!d) return;
    d->login = app->store.login;
    d->by_username = app->add_guard_by_username;
    bm_strlcpy(d->keyword, trimmed, sizeof d->keyword);
    bm_strlcpy(d->title, app->add_guard_title, sizeof d->title);
    app->add_guard_dialog = false;
    app->guard_loading = true;
    if (!bm_job_spawn(add_guard_worker, d)) {
        app->guard_loading = false;
        free(d);
    }
}

static int count_selected_guards(const bm_guard_list *list) {
    int n = 0;
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].selected) n++;
    }
    return n;
}

static void delete_selected_guards(bm_app *app) {
    int removed = 0;
    for (int i = app->store.guards.count - 1; i >= 0; i--) {
        if (app->store.guards.items[i].selected) {
            removed += bm_guard_list_remove_uid(&app->store.original_guards, app->store.guards.items[i].uid);
        }
    }
    bm_refresh_guard_stats_and_filters(app, true);
    if (removed > 0) {
        char msg[64];
        snprintf(msg, sizeof msg, "已删除 %d 位用户", removed);
        bm_toast_push(app, msg, NULL, CUI_TONE_INFO);
    }
}

void bm_page_guard_list(cui_ctx *ui, bm_app *app) {
    if (!app->store.room_id[0]) {
        bm_render_setting_alert(ui, app);
        return;
    }

    cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .gap = 12) {
        /* Top Control Card */
        cui_card(ui, .width = CUI_FILL, .gap = 10, .pad = 16) {
            cui_row(ui, .gap = 8, .align = CUI_ALIGN_CENTER, .wrap = true, .width = CUI_FILL) {
                cui_heading(ui, "编辑列表", 4);
                if (app->guard_loading) cui_spinner(ui, 16);
                cui_spacer(ui);

                char total_str[48], t1_str[48], t2_str[48], t3_str[48];
                snprintf(total_str, sizeof total_str, "总数：%d", app->stats.total);
                snprintf(t1_str, sizeof t1_str, "总督：%d", app->stats.type1);
                snprintf(t2_str, sizeof t2_str, "提督：%d", app->stats.type2);
                snprintf(t3_str, sizeof t3_str, "舰长：%d", app->stats.type3);

                cui_badge(ui, total_str, .tone = CUI_TONE_WARNING, .solid = true);
                if (cui_chip(ui, t1_str, &app->filters.type1, .icon = app->filters.type1 ? CUI_ICON_CIRCLE_CHECK : CUI_ICON_CIRCLE_X)) {
                    bm_refresh_guard_stats_and_filters(app, true);
                }
                if (cui_chip(ui, t2_str, &app->filters.type2, .icon = app->filters.type2 ? CUI_ICON_CIRCLE_CHECK : CUI_ICON_CIRCLE_X)) {
                    bm_refresh_guard_stats_and_filters(app, true);
                }
                if (cui_chip(ui, t3_str, &app->filters.type3, .icon = app->filters.type3 ? CUI_ICON_CIRCLE_CHECK : CUI_ICON_CIRCLE_X)) {
                    bm_refresh_guard_stats_and_filters(app, true);
                }
            }

            cui_secondary(ui, "该列表内的用户将会收到群发私信。列表可生成自某日的 直播间舰长列表快照 或某时间段的 上舰记录（未去重，仅限当前登录账号）");

            /* Toolbar row */
            cui_row(ui, .gap = 6, .align = CUI_ALIGN_CENTER, .wrap = true, .width = CUI_FILL) {
                if (cui_date_picker(ui, "snap_date", &app->snapshot_date, .width = CUI_PX(138))) {
                    char dstr[16];
                    snprintf(dstr, sizeof dstr, "%04d-%02d-%02d", app->snapshot_date.year, app->snapshot_date.month, app->snapshot_date.day);
                    if (bm_date_list_contains(&app->valid_dates, dstr)) {
                        bm_guard_list_fetch_snapshot(app, dstr);
                    } else {
                        bm_toast_push(app, "该日期无历史快照", "已自动为您检查有效快照日期，或点击右侧按钮重试", CUI_TONE_WARNING);
                    }
                }
                if (cui_button(ui, "舰长列表快照", .icon = CUI_ICON_CALENDAR, .size = CUI_SIZE_SM, .loading = app->guard_loading)) {
                    char dstr[16];
                    snprintf(dstr, sizeof dstr, "%04d-%02d-%02d", app->snapshot_date.year, app->snapshot_date.month, app->snapshot_date.day);
                    bm_guard_list_fetch_snapshot(app, dstr);
                }

                if (cui_date_range_picker(ui, "gift_range", &app->gift_range, .placeholder = "选择上舰记录日期范围", .width = CUI_PX(195))) {
                    if (app->gift_range.start.year != 0 && app->gift_range.end.year != 0) {
                        start_fetch_gift_range(app);
                    }
                }
                if (cui_button(ui, "上舰记录", .icon = CUI_ICON_HISTORY, .size = CUI_SIZE_SM, .loading = app->guard_loading)) {
                    start_fetch_gift_range(app);
                }

                cui_spacer(ui);

                if (cui_button(ui, "导入列表", .icon = CUI_ICON_UPLOAD, .size = CUI_SIZE_SM)) {
                    char path[1024] = { 0 };
                    cui_file_dialog_opts fopts = {
                        .kind = CUI_FILE_OPEN,
                        .title = "选择文件",
                        .filter_name = "列表文件 (txt, csv)",
                        .filter = "txt;csv"
                    };
                    if (cui_app_file_dialog(NULL, &fopts, path, sizeof path)) {
                        if (bm_guard_import_file(&app->store.original_guards, path)) {
                            bm_refresh_guard_stats_and_filters(app, true);
                            bm_toast_push(app, "导入成功", path, CUI_TONE_SUCCESS);
                        } else {
                            bm_toast_push(app, "导入失败", "无法读取或解析所选文件", CUI_TONE_DANGER);
                        }
                    }
                }

                cui_menu(ui, "导出列表", .icon = CUI_ICON_DOWNLOAD) {
                    if (cui_menu_item(ui, "导出为 txt 文件", .icon = CUI_ICON_FILE_TEXT)) {
                        char def_name[128], path[1024] = { 0 };
                        snprintf(def_name, sizeof def_name, "%s-%04d-%02d-%02d.txt",
                                 app->store.room_id, app->snapshot_date.year, app->snapshot_date.month, app->snapshot_date.day);
                        cui_file_dialog_opts fopts = {
                            .kind = CUI_FILE_SAVE,
                            .title = "导出为 TXT 文件",
                            .filter_name = "TXT 文件",
                            .filter = "txt",
                            .default_name = def_name
                        };
                        if (cui_app_file_dialog(NULL, &fopts, path, sizeof path)) {
                            if (bm_guard_export_txt(&app->store.guards, path)) {
                                bm_toast_push(app, "导出 TXT 成功", path, CUI_TONE_SUCCESS);
                            }
                        }
                    }
                    if (cui_menu_item(ui, "导出为 csv 文件", .icon = CUI_ICON_FILE)) {
                        char def_name[128], path[1024] = { 0 };
                        snprintf(def_name, sizeof def_name, "%s-%04d-%02d-%02d.csv",
                                 app->store.room_id, app->snapshot_date.year, app->snapshot_date.month, app->snapshot_date.day);
                        cui_file_dialog_opts fopts = {
                            .kind = CUI_FILE_SAVE,
                            .title = "导出为 CSV 文件",
                            .filter_name = "CSV 文件",
                            .filter = "csv",
                            .default_name = def_name
                        };
                        if (cui_app_file_dialog(NULL, &fopts, path, sizeof path)) {
                            if (bm_guard_export_csv(&app->store.guards, path)) {
                                bm_toast_push(app, "导出 CSV 成功", path, CUI_TONE_SUCCESS);
                            }
                        }
                    }
                }

                int sel_count = count_selected_guards(&app->store.guards);
                cui_menu(ui, "编辑列表", .icon = CUI_ICON_SQUARE_PEN, .variant = CUI_BUTTON_PRIMARY) {
                    if (cui_menu_item(ui, "单个添加", .icon = CUI_ICON_USER_PLUS)) {
                        app->add_guard_dialog = true;
                    }
                    if (cui_menu_item(ui, "去重", .icon = CUI_ICON_COPY)) {
                        int removed = bm_guard_deduplicate(&app->store.original_guards);
                        bm_refresh_guard_stats_and_filters(app, true);
                        char msg[64];
                        snprintf(msg, sizeof msg, "移除了 %d 个重复项", removed);
                        bm_toast_push(app, msg, NULL, CUI_TONE_SUCCESS);
                    }
                    char del_label[64];
                    snprintf(del_label, sizeof del_label, "删除 %d 个已选", sel_count);
                    if (cui_menu_item(ui, del_label, .icon = CUI_ICON_TRASH_2, .disabled = sel_count == 0, .danger = true)) {
                        delete_selected_guards(app);
                    }
                    cui_menu_separator(ui);
                    if (cui_menu_item(ui, "清空", .icon = CUI_ICON_TRASH_2, .danger = true)) {
                        bm_guard_list_clear(&app->store.original_guards);
                        bm_refresh_guard_stats_and_filters(app, true);
                        bm_toast_push(app, "列表已清空", NULL, CUI_TONE_INFO);
                    }
                }
            }
        }

        /* Virtual List Card */
        cui_card(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 8, .gap = 0) {
            if (app->store.guards.count == 0) {
                cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER) {
                    if (cui_empty_state(ui, "目标列表为空",
                                        .icon = CUI_ICON_USERS,
                                        .description = "可通过上方「舰长列表快照」、「上舰记录」或「导入列表」生成群发目标",
                                        .action = "获取今日舰长列表")) {
                        char today[16];
                        bm_date_today_str(today, sizeof today);
                        bm_guard_list_fetch_snapshot(app, today);
                    }
                }
            } else {
                int first = 0, last = 0;
                cui_virtual_list(ui, "guard_vlist", app->store.guards.count, 56.0f, CUI_FILL, &first, &last) {
                    for (int i = first; i < last; i++) {
                        bm_guard *g = &app->store.guards.items[i];
                        cui_push_id_int(ui, i);
                        cui_row(ui, .width = CUI_FILL, .height = CUI_PX(56), .pad_x = 12, .gap = 10, .align = CUI_ALIGN_CENTER,
                                .bg = g->selected ? CUI_COLOR_ACCENT_SUBTLE : (cui_color){ 0 }, .radius = 8) {
                            cui_checkbox(ui, "##sel", &g->selected);
                            cui_texture ftex = bm_image_get(ui, g->face, NULL, NULL);
                            cui_avatar(ui, g->username[0] ? g->username : "U", .size = 36, .image = ftex);

                            cui_column(ui, .width = CUI_FILL, .gap = 2) {
                                cui_row(ui, .gap = 8, .align = CUI_ALIGN_CENTER) {
                                    const char *lvl = bm_guard_level_str(g);
                                    cui_tone tone = g->is_alive == 1 ? CUI_TONE_INFO :
                                                    (g->guard_level == 1 ? CUI_TONE_ROSE :
                                                     g->guard_level == 2 ? CUI_TONE_VIOLET : CUI_TONE_WARNING);
                                    cui_badge(ui, lvl, .tone = tone, .size = CUI_SIZE_SM);
                                    cui_text(ui, g->username, .font = CUI_FONT_MEDIUM, .nowrap = true);
                                    char uid_str[32];
                                    snprintf(uid_str, sizeof uid_str, "UID: %llu", (unsigned long long)g->uid);
                                    cui_muted(ui, uid_str);
                                }
                                if (g->time[0]) {
                                    cui_muted(ui, g->time);
                                }
                            }

                            if (cui_icon_button(ui, CUI_ICON_INFO, .tooltip = "打开个人空间")) {
                                char url[256];
                                snprintf(url, sizeof url, "https://space.bilibili.com/%llu", (unsigned long long)g->uid);
                                cui_app_open_url(url);
                            }
                            if (cui_icon_button(ui, CUI_ICON_SEND, .tooltip = "在网页中打开私信")) {
                                char url[256];
                                snprintf(url, sizeof url, "https://message.bilibili.com/#/whisper/mid%llu", (unsigned long long)g->uid);
                                cui_app_open_url(url);
                            }
                        }
                        cui_pop_id(ui);
                    }
                }
            }
        }
    }

    /* Single Add Guard Dialog */
    cui_dialog(ui, "添加用户", &app->add_guard_dialog,
               .description = "手动添加额外用户，默认使用 UID，可选用用户名添加模式；称呼为「总督/提督/舰长」或自定义") {
        cui_column(ui, .gap = 12, .width = CUI_FILL) {
            cui_switch(ui, "用户名添加", &app->add_guard_by_username);
            cui_input(ui, "add_kw", app->add_guard_keyword, sizeof app->add_guard_keyword,
                      .label = app->add_guard_by_username ? "用户名" : "UID",
                      .placeholder = app->add_guard_by_username ? "请输入准确的 B 站昵称" : "请输入数字 UID");
            cui_input(ui, "add_title", app->add_guard_title, sizeof app->add_guard_title,
                      .label = "称呼", .placeholder = "总督 / 提督 / 舰长 或自定义称呼");
        }
        cui_dialog_footer(ui) {
            if (cui_button(ui, "取消")) app->add_guard_dialog = false;
            if (cui_button(ui, "添加", .variant = CUI_BUTTON_PRIMARY)) {
                submit_add_guard(app);
            }
        }
    }
}
