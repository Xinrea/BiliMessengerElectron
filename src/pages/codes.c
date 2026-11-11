#include "ui/app.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int count_selected_codes(const bm_store *s) {
    int n = 0;
    for (int i = 0; i < s->code_count; i++) {
        if (s->codes[i].selected) n++;
    }
    return n;
}

static int count_nonempty_lines(const char *text) {
    if (!text || !text[0]) return 0;
    int count = 0;
    const char *p = text;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == '\r') p++;
        if (*p && *p != '\n') {
            count++;
        }
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
    }
    return count;
}

static bool code_matches_search(const char *code, const char *q) {
    if (!q || !q[0]) return true;
    if (!code) return false;
    size_t nlen = strlen(q);
    for (const char *p = code; *p; p++) {
        size_t i = 0;
        while (i < nlen && p[i] && tolower((unsigned char)p[i]) == tolower((unsigned char)q[i])) {
            i++;
        }
        if (i == nlen) return true;
    }
    return false;
}

void bm_page_codes(cui_ctx *ui, bm_app *app) {
    if (!app->store.room_id[0]) {
        bm_render_setting_alert(ui, app);
        return;
    }

    int code_cnt = app->store.code_count;
    int guard_cnt = app->store.guards.count;

    /* Top Header & Supply/Demand Status Bar */
    cui_row(ui, .width = CUI_FILL, .align = CUI_ALIGN_CENTER, .gap = 10, .wrap = true) {
        cui_heading(ui, "兑换码库存管理", 3);

        char badge_total[64];
        snprintf(badge_total, sizeof badge_total, "当前库存：%d 个", code_cnt);
        cui_badge(ui, badge_total, .tone = CUI_TONE_INFO);

        if (guard_cnt > 0) {
            char diff_txt[96];
            if (code_cnt >= guard_cnt) {
                snprintf(diff_txt, sizeof diff_txt, "可覆盖全部 %d 位目标（预计结余 %d 个）", guard_cnt, code_cnt - guard_cnt);
                cui_badge(ui, diff_txt, .tone = CUI_TONE_SUCCESS, .dot = true);
            } else {
                snprintf(diff_txt, sizeof diff_txt, "目标 %d 人，还差 %d 个兑换码", guard_cnt, guard_cnt - code_cnt);
                cui_badge(ui, diff_txt, .tone = CUI_TONE_DANGER, .dot = true);
            }
        }

        cui_spacer(ui);
        cui_search(ui, "code_search_in", app->code_search, sizeof app->code_search,
                   .placeholder = "搜索兑换码...", .width = CUI_PX(240), .size = CUI_SIZE_SM);
    }

    /* Main Two-Column Workspace */
    cui_row(ui, .width = CUI_FILL, .height = CUI_FILL, .gap = 14, .align = CUI_ALIGN_STRETCH) {
        /* Left Column: Direct Batch Paste & File Import (340px) */
        cui_card(ui, .width = CUI_PX(340), .height = CUI_FILL, .pad = 16, .gap = 12) {
            cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                cui_icon(ui, CUI_ICON_UPLOAD, 16, CUI_COLOR_ACCENT_STRONG);
                cui_gap(ui, 6);
                cui_label(ui, "批量粘贴导入");
                cui_spacer(ui);
                int staged = count_nonempty_lines(app->code_import_buf);
                if (staged > 0) {
                    char st_txt[48];
                    snprintf(st_txt, sizeof st_txt, "待入库 %d 行", staged);
                    cui_badge(ui, st_txt, .tone = CUI_TONE_INFO);
                }
            }

            cui_muted(ui, "每行粘贴一个兑换码，导入时会自动剔除空行与已存在的重复兑换码");

            cui_textarea(ui, "codes_paste_box", app->code_import_buf, sizeof app->code_import_buf,
                         .placeholder = "在此粘贴兑换码列表（一行一个）...\n例如：\nBILI-GIFT-0001\nBILI-GIFT-0002",
                         .height = -1, .width = CUI_FILL);

            cui_row(ui, .gap = 8, .width = CUI_FILL) {
                if (cui_button(ui, "立即导入入库", .icon = CUI_ICON_PLUS, .variant = CUI_BUTTON_PRIMARY,
                               .width = CUI_GROW(1), .disabled = !app->code_import_buf[0])) {
                    int added = bm_store_import_codes(&app->store, app->code_import_buf);
                    bm_store_save(&app->store);
                    app->code_import_buf[0] = '\0';
                    char msg[64];
                    snprintf(msg, sizeof msg, "成功导入 %d 个新兑换码", added);
                    bm_toast_push(app, msg, NULL, CUI_TONE_SUCCESS);
                }
                if (cui_button(ui, "清空输入", .variant = CUI_BUTTON_GHOST, .disabled = !app->code_import_buf[0])) {
                    app->code_import_buf[0] = '\0';
                }
            }

            cui_separator(ui);

            if (cui_button(ui, "从 TXT 文件批量导入...", .icon = CUI_ICON_FOLDER_OPEN, .size = CUI_SIZE_SM, .full_width = true)) {
                char path[1024] = { 0 };
                cui_file_dialog_opts fopts = {
                    .kind = CUI_FILE_OPEN,
                    .title = "选择兑换码 TXT 文件",
                    .filter_name = "TXT 文本文件",
                    .filter = "txt"
                };
                if (cui_app_file_dialog(NULL, &fopts, path, sizeof path)) {
                    size_t len = 0;
                    char *content = NULL;
                    if (bm_read_file(path, &content, &len) && content) {
                        int added = bm_store_import_codes(&app->store, content);
                        free(content);
                        bm_store_save(&app->store);
                        char msg[96];
                        snprintf(msg, sizeof msg, "从文件导入了 %d 个新兑换码", added);
                        bm_toast_push(app, msg, path, CUI_TONE_SUCCESS);
                    } else {
                        free(content);
                        bm_toast_push(app, "读取文件失败", path, CUI_TONE_DANGER);
                    }
                }
            }
        }

        /* Right Column: Inventory Table & Batch Actions */
        cui_card(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 0, .gap = 0) {
            int *vis_indices = NULL;
            int vis_count = 0;
            if (code_cnt > 0) {
                vis_indices = (int *)malloc((size_t)code_cnt * sizeof(int));
                if (vis_indices) {
                    for (int i = 0; i < code_cnt; i++) {
                        if (code_matches_search(app->store.codes[i].code, app->code_search)) {
                            vis_indices[vis_count++] = i;
                        }
                    }
                }
            }

            int sel_count = count_selected_codes(&app->store);

            /* Batch Toolbar */
            cui_row(ui, .width = CUI_FILL, .pad_x = 16, .pad_y = 10, .gap = 8, .align = CUI_ALIGN_CENTER, .bg = CUI_COLOR_SURFACE_MUTED) {
                if (cui_button(ui, sel_count > 0 && sel_count >= vis_count ? "取消全选" : "全选当前",
                               .icon = CUI_ICON_SQUARE_CHECK, .size = CUI_SIZE_SM, .variant = CUI_BUTTON_GHOST,
                               .disabled = vis_count == 0)) {
                    bool target = !(sel_count > 0 && sel_count >= vis_count);
                    for (int k = 0; k < vis_count; k++) {
                        app->store.codes[vis_indices[k]].selected = target;
                    }
                }

                char status_lbl[128];
                snprintf(status_lbl, sizeof status_lbl, "显示 %d / %d 个 · 已选 %d 个", vis_count, code_cnt, sel_count);
                cui_muted(ui, status_lbl);

                cui_spacer(ui);

                char del_lbl[64];
                snprintf(del_lbl, sizeof del_lbl, "删除选中 (%d)###del_codes_btn", sel_count);
                if (cui_button(ui, del_lbl, .icon = CUI_ICON_TRASH_2, .size = CUI_SIZE_SM,
                               .variant = CUI_BUTTON_DANGER, .disabled = sel_count == 0)) {
                    int removed = bm_store_delete_selected_codes(&app->store);
                    bm_store_save(&app->store);
                    char msg[64];
                    snprintf(msg, sizeof msg, "已删除 %d 个兑换码", removed);
                    bm_toast_push(app, msg, NULL, CUI_TONE_INFO);
                }

                if (cui_confirm_button(ui, "清空全部", .confirm_label = "再次点击确认清空",
                                       .icon = CUI_ICON_CIRCLE_X, .size = CUI_SIZE_SM)) {
                    bm_store_clear_codes(&app->store);
                    bm_store_save(&app->store);
                    bm_toast_push(app, "已清空全部兑换码", NULL, CUI_TONE_INFO);
                }
            }

            cui_separator(ui);

            if (vis_count == 0) {
                cui_box(ui, .width = CUI_FILL, .height = CUI_FILL, .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER) {
                    if (app->code_search[0]) {
                        if (cui_empty_state(ui, "未找到匹配的兑换码", .icon = CUI_ICON_SEARCH,
                                            .description = "请尝试其他搜索词或清除筛选", .action = "清除搜索")) {
                            app->code_search[0] = '\0';
                        }
                    } else {
                        cui_empty_state(ui, "兑换码库存为空", .icon = CUI_ICON_GIFT,
                                        .description = "在左侧文本框粘贴兑换码（一行一个）并点击「立即导入入库」，群发含 {code} 的模板时将按顺序自动消耗");
                    }
                }
            } else {
                int first = 0, last = 0;
                int single_del_real_idx = -1;
                cui_virtual_list(ui, "codes_vlist", vis_count, 44.0f, CUI_FILL, &first, &last) {
                    for (int vi = first; vi < last; vi++) {
                        int real_idx = vis_indices[vi];
                        cui_push_id_int(ui, vi);
                        cui_row(ui, .width = CUI_FILL, .height = CUI_PX(44.0f), .pad_x = 16, .gap = 10, .align = CUI_ALIGN_CENTER) {
                            cui_checkbox(ui, "##csel", &app->store.codes[real_idx].selected);
                            char idx_str[32];
                            snprintf(idx_str, sizeof idx_str, "#%d", real_idx + 1);
                            cui_badge(ui, idx_str, .tone = real_idx == 0 ? CUI_TONE_WARNING : CUI_TONE_NEUTRAL);
                            cui_code(ui, app->store.codes[real_idx].code);
                            if (real_idx == 0) {
                                cui_muted(ui, "（下一条发送消耗）");
                            }
                            cui_spacer(ui);
                            cui_copy_button(ui, "copy_code", app->store.codes[real_idx].code);
                            if (cui_icon_button(ui, CUI_ICON_X, .tooltip = "删除该兑换码")) {
                                single_del_real_idx = real_idx;
                            }
                        }
                        cui_pop_id(ui);
                    }
                }
                if (single_del_real_idx >= 0) {
                    for (int k = 0; k < app->store.code_count; k++) {
                        app->store.codes[k].selected = (k == single_del_real_idx);
                    }
                    bm_store_delete_selected_codes(&app->store);
                    bm_store_save(&app->store);
                }
            }

            free(vis_indices);
        }
    }
}
