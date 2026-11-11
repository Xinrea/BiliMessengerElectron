#include "ui/app.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* ---- Async Job 1: Fetch valid snapshot dates ---- */

typedef struct {
    char room_id[64];
    bm_date_list dates;
    bool ok;
} calendar_job_data;

static void on_calendar_loaded(void *raw, void *user) {
    calendar_job_data *d = (calendar_job_data *)raw;
    bm_app *app = (bm_app *)user;
    if (d->ok && strcmp(d->room_id, app->store.room_id) == 0) {
        bm_date_list_free(&app->valid_dates);
        app->valid_dates = d->dates;
        int y, m, day;
        bm_date_today(&y, &m, &day);
        char today_str[16];
        snprintf(today_str, sizeof today_str, "%04d-%02d-%02d", y, m, day);
        bm_date_list_push(&app->valid_dates, today_str);
    } else {
        bm_date_list_free(&d->dates);
    }
    free(d);
}

static void calendar_worker(void *raw) {
    calendar_job_data *d = (calendar_job_data *)raw;
    bm_date_list_init(&d->dates);
    d->ok = bm_bili_get_guard_valid_dates(d->room_id, &d->dates, NULL);
    bm_job_post_main(on_calendar_loaded, d);
}

void bm_guard_list_refresh_calendar(bm_app *app) {
    if (!app || !app->store.room_id[0]) return;
    calendar_job_data *d = (calendar_job_data *)calloc(1, sizeof *d);
    if (!d) return;
    bm_strlcpy(d->room_id, app->store.room_id, sizeof d->room_id);
    if (!bm_job_spawn(calendar_worker, d)) {
        free(d);
    }
}

/* ---- Async Job 2: Fetch snapshot guard list ---- */

typedef struct {
    char room_id[64];
    char date_str[16];
    bm_guard_list list;
    bool ok;
    bm_bili_err err;
} snapshot_job_data;

static void on_snapshot_loaded(void *raw, void *user) {
    snapshot_job_data *d = (snapshot_job_data *)raw;
    bm_app *app = (bm_app *)user;
    app->guard_loading = false;
    if (d->ok) {
        bm_guard_list_copy(&app->store.original_guards, &d->list);
        bm_refresh_guard_stats_and_filters(app, true);
        char msg[128];
        snprintf(msg, sizeof msg, "已加载 %s 快照（共 %d 人）", d->date_str, d->list.count);
        bm_toast_push(app, msg, NULL, CUI_TONE_SUCCESS);
    } else {
        bm_toast_push(app, "获取舰长列表快照失败", d->err.message, CUI_TONE_DANGER);
    }
    bm_guard_list_free(&d->list);
    free(d);
}

static void snapshot_worker(void *raw) {
    snapshot_job_data *d = (snapshot_job_data *)raw;
    bm_guard_list_init(&d->list);
    d->ok = bm_bili_get_guard_snapshot(d->room_id, d->date_str, &d->list, &d->err);
    bm_job_post_main(on_snapshot_loaded, d);
}

void bm_guard_list_fetch_snapshot(bm_app *app, const char *date_str) {
    if (!app || !app->store.room_id[0] || app->guard_loading) return;
    snapshot_job_data *d = (snapshot_job_data *)calloc(1, sizeof *d);
    if (!d) return;
    bm_strlcpy(d->room_id, app->store.room_id, sizeof d->room_id);
    bm_strlcpy(d->date_str, date_str, sizeof d->date_str);
    app->guard_loading = true;
    if (!bm_job_spawn(snapshot_worker, d)) {
        app->guard_loading = false;
        free(d);
    }
}

void bm_guard_list_refresh_all(bm_app *app) {
    bm_guard_list_refresh_calendar(app);
    int y, m, d;
    bm_date_today(&y, &m, &d);
    char today[16];
    snprintf(today, sizeof today, "%04d-%02d-%02d", y, m, d);
    bm_guard_list_fetch_snapshot(app, today);
}

/* ---- Async Job 3: Fetch received gift stream ---- */

typedef struct {
    bm_login_info login;
    char begin_date[16];
    char end_date[16];
    bm_guard_list list;
    bool ok;
    bm_bili_err err;
} gift_job_data;

static void on_gift_loaded(void *raw, void *user) {
    gift_job_data *d = (gift_job_data *)raw;
    bm_app *app = (bm_app *)user;
    app->guard_loading = false;
    if (d->ok) {
        bm_guard_list_copy(&app->store.original_guards, &d->list);
        bm_refresh_guard_stats_and_filters(app, true);
        char msg[128];
        snprintf(msg, sizeof msg, "上舰记录拉取完成（共 %d 条）", d->list.count);
        bm_toast_push(app, msg, NULL, CUI_TONE_SUCCESS);
    } else {
        if (d->err.code == -101) {
            bm_store_clear_login(&app->store);
            memset(&app->account_profile, 0, sizeof app->account_profile);
            bm_store_save(&app->store);
        }
        bm_toast_push(app, "获取上舰记录失败", d->err.message, CUI_TONE_DANGER);
    }
    bm_guard_list_free(&d->list);
    free(d);
}

static void gift_worker(void *raw) {
    gift_job_data *d = (gift_job_data *)raw;
    bm_guard_list_init(&d->list);
    d->ok = bm_bili_get_received_guards_by_period(&d->login, d->begin_date, d->end_date, &d->list, &d->err);
    bm_job_post_main(on_gift_loaded, d);
}

static void start_fetch_gift_range(bm_app *app) {
    if (app->guard_loading) return;
    if (!app->store.login.has_login) {
        bm_toast_push(app, "请先前往设置页登录", NULL, CUI_TONE_WARNING);
        return;
    }
    if (app->gift_range.start.year == 0 || app->gift_range.end.year == 0) {
        bm_toast_push(app, "请先选择有效的起止日期范围", NULL, CUI_TONE_WARNING);
        return;
    }
    char bstr[16], estr[16];
    snprintf(bstr, sizeof bstr, "%04d-%02d-%02d",
             app->gift_range.start.year, app->gift_range.start.month, app->gift_range.start.day);
    snprintf(estr, sizeof estr, "%04d-%02d-%02d",
             app->gift_range.end.year, app->gift_range.end.month, app->gift_range.end.day);

    gift_job_data *d = (gift_job_data *)calloc(1, sizeof *d);
    if (!d) return;
    d->login = app->store.login;
    bm_strlcpy(d->begin_date, bstr, sizeof d->begin_date);
    bm_strlcpy(d->end_date, estr, sizeof d->end_date);
    app->guard_loading = true;
    if (!bm_job_spawn(gift_worker, d)) {
        app->guard_loading = false;
        free(d);
    }
}

static void set_gift_range_preset(bm_app *app, int mode) {
    int y, m, d;
    bm_date_today(&y, &m, &d);
    if (mode == 0) {
        /* This month */
        app->gift_range.start = (cui_date){ y, m, 1 };
        app->gift_range.end = (cui_date){ y, m, d };
    } else if (mode == 1) {
        /* Last month */
        int ly = (m == 1) ? (y - 1) : y;
        int lm = (m == 1) ? 12 : (m - 1);
        app->gift_range.start = (cui_date){ ly, lm, 1 };
        app->gift_range.end = (cui_date){ ly, lm, bm_days_in_month(ly, lm) };
    } else if (mode == 2) {
        /* Last 7 days */
        cui_date end_d = { y, m, d };
        int days = cui_date_to_days(end_d);
        app->gift_range.start = cui_date_from_days(days - 6);
        app->gift_range.end = end_d;
    }
}

/* ---- Async Job 4: Add single guard by UID or username ---- */

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
    if (!d->ok) {
        bm_toast_push(app, d->err.message[0] ? d->err.message : "未找到该用户", NULL, CUI_TONE_DANGER);
        free(d);
        return;
    }
    if (bm_guard_list_find_uid(&app->store.original_guards, d->prof.mid) >= 0) {
        char msg[200];
        snprintf(msg, sizeof msg, "用户 %s (%llu) 已在列表中", d->prof.uname, (unsigned long long)d->prof.mid);
        bm_toast_push(app, msg, NULL, CUI_TONE_WARNING);
        free(d);
        return;
    }
    bm_guard g;
    memset(&g, 0, sizeof g);
    g.uid = d->prof.mid;
    bm_strlcpy(g.username, d->prof.uname, sizeof g.username);
    bm_strlcpy(g.face, d->prof.face, sizeof g.face);
    g.guard_level = d->title[0] ? 0 : 3;
    bm_strlcpy(g.custom_title, d->title, sizeof g.custom_title);
    bm_guard_list_unshift(&app->store.original_guards, &g);
    bm_refresh_guard_stats_and_filters(app, true);
    app->add_guard_keyword[0] = '\0';
    app->add_guard_title[0] = '\0';
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

static bool contains_icase(const char *haystack, const char *needle) {
    if (!needle || !needle[0]) return true;
    if (!haystack) return false;
    size_t nlen = strlen(needle);
    for (const char *p = haystack; *p; p++) {
        size_t i = 0;
        while (i < nlen && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)needle[i])) {
            i++;
        }
        if (i == nlen) return true;
    }
    return false;
}

static bool guard_matches_search(const bm_guard *g, const char *q) {
    if (!q || !q[0]) return true;
    if (contains_icase(g->username, q)) return true;
    if (contains_icase(g->custom_title, q)) return true;
    char uid_buf[32];
    snprintf(uid_buf, sizeof uid_buf, "%llu", (unsigned long long)g->uid);
    return contains_icase(uid_buf, q);
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

static void export_txt_action(bm_app *app) {
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

static void export_csv_action(bm_app *app) {
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

static void open_import_file_dialog(bm_app *app) {
    char path[1024] = { 0 };
    cui_file_dialog_opts fopts = {
        .kind = CUI_FILE_OPEN,
        .title = "选择舰长列表文件",
        .filter_name = "列表文件 (txt, csv)",
        .filter = "txt;csv"
    };
    if (cui_app_file_dialog(NULL, &fopts, path, sizeof path)) {
        if (bm_guard_import_file(&app->store.original_guards, path)) {
            bm_refresh_guard_stats_and_filters(app, true);
            bm_toast_push(app, "导入列表成功", path, CUI_TONE_SUCCESS);
        } else {
            bm_toast_push(app, "导入失败", "无法读取或解析所选文件", CUI_TONE_DANGER);
        }
    }
}

void bm_page_guard_list(cui_ctx *ui, bm_app *app) {
    if (!app->store.room_id[0]) {
        bm_render_setting_alert(ui, app);
        return;
    }

    /* Top Header & Filter Bar */
    cui_row(ui, .width = CUI_FILL, .align = CUI_ALIGN_CENTER, .gap = 10, .wrap = true) {
        cui_row(ui, .gap = 8, .align = CUI_ALIGN_CENTER) {
            cui_heading(ui, "目标列表工作台", 3);
            if (app->guard_loading) cui_spinner(ui, 18);
        }

        char total_str[48], t1_str[48], t2_str[48], t3_str[48];
        snprintf(total_str, sizeof total_str, "全部 %d", app->stats.total);
        snprintf(t1_str, sizeof t1_str, "总督 %d", app->stats.type1);
        snprintf(t2_str, sizeof t2_str, "提督 %d", app->stats.type2);
        snprintf(t3_str, sizeof t3_str, "舰长 %d", app->stats.type3);

        cui_badge(ui, total_str, .tone = CUI_TONE_NEUTRAL);
        if (bm_guard_filter_chip(ui, "flt_guard_1", 1, t1_str, &app->filters.type1)) {
            bm_refresh_guard_stats_and_filters(app, true);
        }
        if (bm_guard_filter_chip(ui, "flt_guard_2", 2, t2_str, &app->filters.type2)) {
            bm_refresh_guard_stats_and_filters(app, true);
        }
        if (bm_guard_filter_chip(ui, "flt_guard_3", 3, t3_str, &app->filters.type3)) {
            bm_refresh_guard_stats_and_filters(app, true);
        }

        cui_spacer(ui);

        /* Quick actions right on top header */
        if (cui_button(ui, "今日实时", .icon = CUI_ICON_ZAP, .size = CUI_SIZE_MD,
                       .variant = CUI_BUTTON_SECONDARY,
                       .tooltip = "快速拉取今日实时去重舰长快照", .loading = app->guard_loading)) {
            int y, m, d;
            bm_date_today(&y, &m, &d);
            app->snapshot_date = (cui_date){ y, m, d };
            char dstr[16];
            snprintf(dstr, sizeof dstr, "%04d-%02d-%02d", y, m, d);
            bm_guard_list_fetch_snapshot(app, dstr);
        }
        if (cui_button(ui, "获取 / 导入名单", .icon = CUI_ICON_DOWNLOAD, .size = CUI_SIZE_MD,
                       .variant = CUI_BUTTON_PRIMARY, .tooltip = "打开导入弹窗（支持历史快照、礼物流水、TXT/CSV 文件与手动添加）")) {
            app->import_guard_dialog = true;
        }
    }

    /* Main Full-Width Guard List Table Card */
    cui_card(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 0, .gap = 0) {
        /* Build filtered view index mapping */
        int total_guards = app->store.guards.count;
        int *vis_indices = NULL;
        int vis_count = 0;
        if (total_guards > 0) {
            vis_indices = (int *)malloc((size_t)total_guards * sizeof(int));
            if (vis_indices) {
                for (int i = 0; i < total_guards; i++) {
                    if (guard_matches_search(&app->store.guards.items[i], app->guard_search)) {
                        vis_indices[vis_count++] = i;
                    }
                }
            }
        }

        int sel_count = count_selected_guards(&app->store.guards);

        /* Batch Action & Search Toolbar inside Card */
        cui_row(ui, .width = CUI_FILL, .pad_x = 16, .pad_y = 10, .gap = 8, .wrap = true,
                .align = CUI_ALIGN_CENTER, .bg = CUI_COLOR_SURFACE_MUTED) {
            if (cui_button(ui, sel_count > 0 && sel_count >= vis_count ? "取消全选" : "全选当前",
                           .icon = CUI_ICON_SQUARE_CHECK, .size = CUI_SIZE_MD, .variant = CUI_BUTTON_SECONDARY,
                           .disabled = vis_count == 0)) {
                bool target_state = !(sel_count > 0 && sel_count >= vis_count);
                for (int k = 0; k < vis_count; k++) {
                    bm_guard *g = &app->store.guards.items[vis_indices[k]];
                    g->selected = target_state;
                    int oidx = bm_guard_list_find_uid(&app->store.original_guards, g->uid);
                    if (oidx >= 0) app->store.original_guards.items[oidx].selected = target_state;
                }
            }

            cui_spacer(ui);

            cui_search(ui, "guard_search_input", app->guard_search, sizeof app->guard_search,
                       .placeholder = "搜索昵称 / UID / 头衔...", .width = CUI_PX(220), .size = CUI_SIZE_MD);

            cui_gap(ui, 4);

            if (cui_button(ui, "一键去重", .icon = CUI_ICON_FILTER, .size = CUI_SIZE_MD,
                           .variant = CUI_BUTTON_SECONDARY,
                           .tooltip = "按 UID 去重并保留最高大航海等级", .disabled = total_guards <= 1)) {
                int removed = bm_guard_deduplicate(&app->store.original_guards);
                bm_refresh_guard_stats_and_filters(app, true);
                char msg[64];
                snprintf(msg, sizeof msg, "去重完成，移除了 %d 条重复记录", removed);
                bm_toast_push(app, msg, NULL, CUI_TONE_SUCCESS);
            }

            cui_menu(ui, "导出名单", .icon = CUI_ICON_SHARE_2, .variant = CUI_BUTTON_SECONDARY) {
                if (cui_menu_item(ui, "导出为 TXT 文件", .icon = CUI_ICON_FILE_TEXT)) {
                    export_txt_action(app);
                }
                if (cui_menu_item(ui, "导出为 CSV 表格", .icon = CUI_ICON_FILE)) {
                    export_csv_action(app);
                }
            }

            char del_btn_lbl[64];
            snprintf(del_btn_lbl, sizeof del_btn_lbl, "删除选中 (%d)###del_sel_guards", sel_count);
            if (cui_button(ui, del_btn_lbl, .icon = CUI_ICON_TRASH_2, .size = CUI_SIZE_MD,
                           .variant = CUI_BUTTON_SECONDARY, .disabled = sel_count == 0)) {
                delete_selected_guards(app);
            }

            if (total_guards > 0) {
                if (cui_confirm_button(ui, "清空列表", .confirm_label = "再次点击确认清空",
                                       .icon = CUI_ICON_CIRCLE_X, .size = CUI_SIZE_MD,
                                       .variant = CUI_BUTTON_SECONDARY)) {
                    bm_guard_list_clear(&app->store.original_guards);
                    bm_refresh_guard_stats_and_filters(app, true);
                    bm_toast_push(app, "已清空目标列表", NULL, CUI_TONE_INFO);
                }
            }
        }
        cui_row(ui, .width = CUI_FILL, .pad_x = 16, .pad_bottom = 10) {
            char status_lbl[128];
            if (app->guard_search[0]) {
                snprintf(status_lbl, sizeof status_lbl, "筛选出 %d / %d 人 · 已选 %d 人", vis_count, total_guards, sel_count);
            } else {
                snprintf(status_lbl, sizeof status_lbl, "展示 %d 人 · 已选 %d 人", vis_count, sel_count);
            }
            cui_muted(ui, status_lbl);
        }

        cui_separator(ui);

        /* Virtual List Body */
        if (vis_count == 0) {
            cui_box(ui, .width = CUI_FILL, .height = CUI_FILL, .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER) {
                if (app->guard_search[0]) {
                    if (cui_empty_state(ui, "未找到匹配的目标用户", .icon = CUI_ICON_SEARCH,
                                        .description = "请尝试更换搜索关键词或清除搜索框", .action = "清除搜索")) {
                        app->guard_search[0] = '\0';
                    }
                } else {
                    if (cui_empty_state(ui, "目标列表为空", .icon = CUI_ICON_USERS,
                                        .description = "点击下方快速获取今日舰长，或打开导入弹窗获取历史快照、流水及文件",
                                        .action = "获取与导入名单")) {
                        app->import_guard_dialog = true;
                    }
                }
            }
        } else {
            int first = 0, last = 0;
            uint64_t single_remove_uid = 0;
            cui_virtual_list(ui, "guards_vlist", vis_count, 54.0f, CUI_FILL, &first, &last) {
                for (int vi = first; vi < last; vi++) {
                    int real_idx = vis_indices[vi];
                    bm_guard *g = &app->store.guards.items[real_idx];
                    cui_push_id_int(ui, vi);
                    cui_row(ui, .width = CUI_FILL, .height = CUI_PX(54.0f), .pad_x = 16, .gap = 10, .align = CUI_ALIGN_CENTER) {
                        if (cui_checkbox(ui, "##sel", &g->selected)) {
                            int oidx = bm_guard_list_find_uid(&app->store.original_guards, g->uid);
                            if (oidx >= 0) {
                                app->store.original_guards.items[oidx].selected = g->selected;
                            }
                        }

                        cui_texture ftex = bm_image_get(ui, g->face, NULL, NULL);
                        cui_avatar(ui, g->username[0] ? g->username : "U", .size = 36, .image = ftex);

                        char uid_txt[48], uid_raw[32];
                        snprintf(uid_raw, sizeof uid_raw, "%llu", (unsigned long long)g->uid);
                        snprintf(uid_txt, sizeof uid_txt, "UID %s", uid_raw);

                        cui_column(ui, .gap = 2, .width = CUI_FILL) {
                            cui_row(ui, .gap = 6, .align = CUI_ALIGN_CENTER) {
                                bm_guard_badge(ui, g);
                                cui_text(ui, g->username, .font = CUI_FONT_MEDIUM, .size = 14, .nowrap = true);
                                if (g->custom_title[0]) {
                                    cui_badge(ui, g->custom_title, .tone = CUI_TONE_NEUTRAL);
                                }
                            }
                            cui_muted(ui, uid_txt);
                        }

                        cui_copy_button(ui, "copy_uid", uid_raw);

                        if (cui_button(ui, "空间", .icon = CUI_ICON_EXTERNAL_LINK, .variant = CUI_BUTTON_GHOST, .size = CUI_SIZE_SM)) {
                            char url[256];
                            snprintf(url, sizeof url, "https://space.bilibili.com/%llu", (unsigned long long)g->uid);
                            cui_app_open_url(url);
                        }
                        if (cui_button(ui, "私信", .icon = CUI_ICON_SEND, .variant = CUI_BUTTON_GHOST, .size = CUI_SIZE_SM)) {
                            char url[256];
                            snprintf(url, sizeof url, "https://message.bilibili.com/#/whisper/mid%llu", (unsigned long long)g->uid);
                            cui_app_open_url(url);
                        }
                        if (cui_icon_button(ui, CUI_ICON_X, .tooltip = "从列表中移除")) {
                            single_remove_uid = g->uid;
                        }
                    }
                    cui_pop_id(ui);
                }
            }
            if (single_remove_uid != 0) {
                bm_guard_list_remove_uid(&app->store.original_guards, single_remove_uid);
                bm_refresh_guard_stats_and_filters(app, true);
            }
        }

        free(vis_indices);
    }

    /* Unified Import & Fetch Dialog */
    cui_dialog(ui, "获取与导入目标名单", &app->import_guard_dialog,
               .description = "选择数据来源以加载或合并目标舰长名单", .width = 540) {
        cui_column(ui, .gap = 14, .width = CUI_FILL) {
            static const char *tabs[] = { "历史快照", "上舰流水", "文件导入", "手动添加" };
            static const char *tab_icons[] = { CUI_ICON_CALENDAR, CUI_ICON_HISTORY, CUI_ICON_FOLDER_OPEN, CUI_ICON_USER_PLUS };
            cui_segmented(ui, "import_tabs_seg", tabs, 4, &app->import_guard_tab, .icons = tab_icons, .full_width = true);

            if (app->import_guard_tab == 0) {
                /* Source 1: Snapshot */
                cui_column(ui, .gap = 12, .width = CUI_FILL) {
                    cui_box(ui, .width = CUI_FILL, .pad = 12, .radius = 8, .bg = CUI_COLOR_SURFACE_MUTED, .gap = 6) {
                        cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                            cui_column(ui, .gap = 2, .width = CUI_FILL) {
                                cui_text(ui, "今日实时快照（推荐）", .font = CUI_FONT_MEDIUM);
                                cui_muted(ui, "直接拉取当前直播间最新去重舰长榜单数据");
                            }
                            if (cui_button(ui, "拉取今日快照", .icon = CUI_ICON_ZAP, .variant = CUI_BUTTON_PRIMARY,
                                           .size = CUI_SIZE_SM, .loading = app->guard_loading)) {
                                int y, m, d;
                                bm_date_today(&y, &m, &d);
                                app->snapshot_date = (cui_date){ y, m, d };
                                char dstr[16];
                                snprintf(dstr, sizeof dstr, "%04d-%02d-%02d", y, m, d);
                                bm_guard_list_fetch_snapshot(app, dstr);
                                app->import_guard_dialog = false;
                            }
                        }
                    }

                    cui_separator(ui);

                    cui_label(ui, "或者选择历史日期拉取历史快照：");
                    cui_row(ui, .gap = 8, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                        cui_date_picker(ui, "snap_date_picker", &app->snapshot_date, .width = CUI_FILL);
                        char sel_dstr[16];
                        snprintf(sel_dstr, sizeof sel_dstr, "%04d-%02d-%02d",
                                 app->snapshot_date.year, app->snapshot_date.month, app->snapshot_date.day);
                        bool date_ok = bm_date_list_contains(&app->valid_dates, sel_dstr);
                        cui_badge(ui, date_ok ? "有快照" : "未检测到快照", .tone = date_ok ? CUI_TONE_SUCCESS : CUI_TONE_NEUTRAL);
                    }
                    cui_muted(ui, "支持从 guard.vjoi.cn 存档服务查询并拉取指定日期的去重历史大航海名单。");

                    if (cui_button(ui, "拉取所选日期快照", .icon = CUI_ICON_DOWNLOAD, .variant = CUI_BUTTON_SECONDARY,
                                   .full_width = true, .loading = app->guard_loading)) {
                        char dstr[16];
                        snprintf(dstr, sizeof dstr, "%04d-%02d-%02d",
                                 app->snapshot_date.year, app->snapshot_date.month, app->snapshot_date.day);
                        bm_guard_list_fetch_snapshot(app, dstr);
                        app->import_guard_dialog = false;
                    }
                }
            } else if (app->import_guard_tab == 1) {
                /* Source 2: Gift Stream */
                cui_column(ui, .gap = 12, .width = CUI_FILL) {
                    cui_muted(ui, "拉取当前登录主播在时间段内的全部上舰礼物记录（含重复续费，拉取后可一键去重合并）：");

                    cui_row(ui, .gap = 6, .width = CUI_FILL) {
                        if (cui_button(ui, "本月流水", .size = CUI_SIZE_SM, .variant = CUI_BUTTON_OUTLINE, .width = CUI_GROW(1))) {
                            set_gift_range_preset(app, 0);
                        }
                        if (cui_button(ui, "上月流水", .size = CUI_SIZE_SM, .variant = CUI_BUTTON_OUTLINE, .width = CUI_GROW(1))) {
                            set_gift_range_preset(app, 1);
                        }
                        if (cui_button(ui, "近 7 天流水", .size = CUI_SIZE_SM, .variant = CUI_BUTTON_OUTLINE, .width = CUI_GROW(1))) {
                            set_gift_range_preset(app, 2);
                        }
                    }

                    cui_date_range_picker(ui, "gift_range_picker", &app->gift_range,
                                          .placeholder = "选择开始与结束日期", .width = CUI_FILL);

                    if (cui_button(ui, "开始拉取区间上舰流水", .icon = CUI_ICON_HISTORY,
                                   .variant = CUI_BUTTON_PRIMARY, .full_width = true, .loading = app->guard_loading)) {
                        start_fetch_gift_range(app);
                        app->import_guard_dialog = false;
                    }
                }
            } else if (app->import_guard_tab == 2) {
                /* Source 3: File Import */
                cui_column(ui, .gap = 12, .width = CUI_FILL) {
                    cui_muted(ui, "支持 TXT（支持单行 UID、或逗号分隔 UID,昵称）及 CSV 文件导入：");

                    int dz = cui_dropzone(ui, "guard_file_dropzone",
                                          .title = "拖拽 TXT / CSV 导入",
                                          .description = "或点击浏览文件",
                                          .accept = "txt;csv",
                                          .width = CUI_FILL);
                    if (dz == -1) {
                        open_import_file_dialog(app);
                        app->import_guard_dialog = false;
                    } else if (dz > 0) {
                        const char *dropped = cui_dropped_file(ui, 0);
                        if (dropped && bm_guard_import_file(&app->store.original_guards, dropped)) {
                            bm_refresh_guard_stats_and_filters(app, true);
                            bm_toast_push(app, "拖拽导入成功", dropped, CUI_TONE_SUCCESS);
                            app->import_guard_dialog = false;
                        } else {
                            bm_toast_push(app, "导入失败", "请使用有效的 TXT 或 CSV 文件", CUI_TONE_DANGER);
                        }
                    }
                }
            } else if (app->import_guard_tab == 3) {
                /* Source 4: Manual Add Single */
                cui_column(ui, .gap = 12, .width = CUI_FILL) {
                    static const char *search_modes[] = { "按 UID 精确查找", "按用户名搜索 (需已登录)" };
                    int mode_idx = app->add_guard_by_username ? 1 : 0;
                    if (cui_segmented(ui, "add_guard_mode", search_modes, 2, &mode_idx, .full_width = true)) {
                        app->add_guard_by_username = (mode_idx == 1);
                    }
                    cui_input(ui, "add_guard_kw", app->add_guard_keyword, sizeof app->add_guard_keyword,
                              .label = app->add_guard_by_username ? "Bilibili 用户名" : "Bilibili UID",
                              .placeholder = app->add_guard_by_username ? "输入完整昵称搜索" : "例如：475210");
                    cui_input(ui, "add_guard_title", app->add_guard_title, sizeof app->add_guard_title,
                              .label = "自定义头衔（可选）",
                              .placeholder = "留空则默认为「舰长」，填入可替换模板中的 {level}");

                    if (cui_button(ui, "确认添加并加入列表", .variant = CUI_BUTTON_PRIMARY, .icon = CUI_ICON_PLUS, .full_width = true)) {
                        submit_add_guard(app);
                        app->import_guard_dialog = false;
                    }
                }
            }
        }
        cui_dialog_footer(ui) {
            if (cui_button(ui, "关闭")) {
                app->import_guard_dialog = false;
            }
        }
    }
}
