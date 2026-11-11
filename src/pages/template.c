#include "ui/app.h"

#include <stdio.h>
#include <string.h>

static void sync_msg_template_selection(bm_app *app) {
    if (app->store.template_count <= 0) {
        app->msg_template_idx = -1;
        app->msg_template_name[0] = '\0';
        return;
    }
    if (app->msg_template_name[0]) {
        for (int i = 0; i < app->store.template_count; i++) {
            if (strcmp(app->store.templates[i].name, app->msg_template_name) == 0) {
                app->msg_template_idx = i;
                return;
            }
        }
    }
    app->msg_template_idx = 0;
    bm_strlcpy(app->msg_template_name, app->store.templates[0].name, sizeof app->msg_template_name);
}

static void append_tag(bm_app *app, const char *tag) {
    size_t cur = strlen(app->template_value_buf);
    size_t tlen = strlen(tag);
    if (cur + tlen + 1 < sizeof app->template_value_buf) {
        memcpy(app->template_value_buf + cur, tag, tlen + 1);
        app->template_changed = true;
    }
}

static void start_new_template(bm_app *app) {
    app->template_select_idx = -1;
    snprintf(app->template_name_buf, sizeof app->template_name_buf, "新模板 %d", app->store.template_count + 1);
    bm_strlcpy(app->template_value_buf, "你好，{level} {name}！感谢你在 {month} 月对直播间的支持～", sizeof app->template_value_buf);
    app->template_changed = true;
}

void bm_page_template(cui_ctx *ui, bm_app *app) {
    if (!app->store.room_id[0]) {
        bm_render_setting_alert(ui, app);
        return;
    }

    /* Top Header */
    cui_row(ui, .width = CUI_FILL, .align = CUI_ALIGN_CENTER, .gap = 10) {
        cui_heading(ui, "私信模板工作坊", 3);
        char cnt_badge[32];
        snprintf(cnt_badge, sizeof cnt_badge, "共 %d 个模板", app->store.template_count);
        cui_badge(ui, cnt_badge, .tone = CUI_TONE_WARNING);
        if (app->template_changed) {
            cui_badge(ui, "未保存更改", .tone = CUI_TONE_INFO, .dot = true);
        }
        cui_spacer(ui);
        if (cui_button(ui, "新建模板", .icon = CUI_ICON_PLUS, .size = CUI_SIZE_SM, .variant = CUI_BUTTON_PRIMARY)) {
            start_new_template(app);
        }
    }

    /* Main 3-Column Studio */
    cui_row(ui, .width = CUI_FILL, .height = CUI_FILL, .gap = 14, .align = CUI_ALIGN_STRETCH) {
        /* Column 1: Template Library List (230px) */
        cui_card(ui, .width = CUI_PX(230), .height = CUI_FILL, .pad = 12, .gap = 8) {
            cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                cui_icon(ui, CUI_ICON_FOLDER, 15, CUI_COLOR_ACCENT_STRONG);
                cui_gap(ui, 6);
                cui_label(ui, "已保存模板库");
            }
            cui_separator(ui);

            if (app->store.template_count == 0) {
                cui_box(ui, .width = CUI_FILL, .height = CUI_FILL, .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER) {
                    if (cui_empty_state(ui, "暂无模板", .icon = CUI_ICON_FILE_TEXT,
                                        .description = "点击新建创建第一个群发模板", .action = "新建模板")) {
                        start_new_template(app);
                    }
                }
            } else {
                cui_scroll(ui, .id = "tpl_list_scroll", .width = CUI_FILL, .height = CUI_FILL, .gap = 4) {
                    for (int i = 0; i < app->store.template_count; i++) {
                        cui_push_id_int(ui, i);
                        const bm_template *t = &app->store.templates[i];
                        char snippet[96];
                        bm_strlcpy(snippet, t->value, sizeof snippet);
                        for (char *p = snippet; *p; p++) {
                            if (*p == '\n' || *p == '\r') *p = ' ';
                        }
                        bool has_code = strstr(t->value, "{code}") != NULL;
                        if (cui_list_item(ui, t->name,
                                          .icon = CUI_ICON_FILE_TEXT,
                                          .subtitle = snippet[0] ? snippet : "(空内容)",
                                          .trailing = has_code ? "{code}" : NULL,
                                          .selected = app->template_select_idx == i)) {
                            app->template_select_idx = i;
                            bm_strlcpy(app->template_name_buf, t->name, sizeof app->template_name_buf);
                            bm_strlcpy(app->template_value_buf, t->value, sizeof app->template_value_buf);
                            app->template_changed = false;
                        }
                        cui_pop_id(ui);
                    }
                }
            }
        }

        /* Column 2: Template Editor (GROW 6) */
        cui_card(ui, .width = CUI_GROW(6), .height = CUI_FILL, .pad = 16, .gap = 12) {
            cui_row(ui, .gap = 8, .align = CUI_ALIGN_END, .width = CUI_FILL) {
                if (cui_input(ui, "tpl_name_in", app->template_name_buf, sizeof app->template_name_buf,
                              .label = "模板名称", .placeholder = "输入模板名称（保存时作为唯一标识）", .width = CUI_FILL) & CUI_CHANGED) {
                    app->template_changed = true;
                }
                if (cui_button(ui, "保存模板", .icon = CUI_ICON_SAVE, .variant = CUI_BUTTON_PRIMARY,
                               .disabled = !app->template_changed || !app->template_name_buf[0])) {
                    bm_store_set_template(&app->store, app->template_name_buf, app->template_value_buf);
                    bm_store_save(&app->store);
                    for (int i = 0; i < app->store.template_count; i++) {
                        if (strcmp(app->store.templates[i].name, app->template_name_buf) == 0) {
                            app->template_select_idx = i;
                            break;
                        }
                    }
                    sync_msg_template_selection(app);
                    app->template_changed = false;
                    bm_toast_push(app, "模板已保存", app->template_name_buf, CUI_TONE_SUCCESS);
                }
                if (cui_confirm_button(ui, "删除", .confirm_label = "确认删除", .icon = CUI_ICON_TRASH_2)) {
                    if (app->template_name_buf[0] && bm_store_delete_template(&app->store, app->template_name_buf)) {
                        bm_store_save(&app->store);
                        bm_toast_push(app, "模板已删除", app->template_name_buf, CUI_TONE_INFO);
                        if (app->store.template_count > 0) {
                            app->template_select_idx = 0;
                            bm_strlcpy(app->template_name_buf, app->store.templates[0].name, sizeof app->template_name_buf);
                            bm_strlcpy(app->template_value_buf, app->store.templates[0].value, sizeof app->template_value_buf);
                        } else {
                            app->template_select_idx = -1;
                            app->template_name_buf[0] = '\0';
                            app->template_value_buf[0] = '\0';
                        }
                        sync_msg_template_selection(app);
                        app->template_changed = false;
                    }
                }
            }

            /* Quick Tag Insertion Bar */
            cui_box(ui, .width = CUI_FILL, .pad = 10, .radius = 8, .bg = CUI_COLOR_SURFACE_MUTED, .gap = 6) {
                cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                    cui_muted(ui, "点击快捷插入动态变量标签：");
                    cui_spacer(ui);
                    if (strstr(app->template_value_buf, "{code}") != NULL) {
                        char code_tip[64];
                        snprintf(code_tip, sizeof code_tip, "已启用兑换码（库存 %d）", app->store.code_count);
                        cui_badge(ui, code_tip, .tone = app->store.code_count > 0 ? CUI_TONE_SUCCESS : CUI_TONE_WARNING);
                    }
                }
                cui_row(ui, .gap = 6, .wrap = true, .width = CUI_FILL) {
                    static const struct { const char *tag; const char *tip; } tags[] = {
                        { "{name}",      "插入目标用户 Bilibili 昵称" },
                        { "{level}",     "插入大航海等级（总督 / 提督 / 舰长 / 自定义头衔）" },
                        { "{code}",      "插入并自动消耗一个兑换码" },
                        { "{date}",      "插入当前完整日期（YYYY-MM-DD）" },
                        { "{month}",     "插入当前月份数字（1-12）" },
                        { "{lastMonth}", "插入上个月份数字（1-12）" },
                        { "{day}",       "插入当前日期号数（1-31）" }
                    };
                    for (int i = 0; i < 7; i++) {
                        if (cui_button(ui, tags[i].tag, .size = CUI_SIZE_SM, .variant = CUI_BUTTON_SECONDARY, .tooltip = tags[i].tip)) {
                            append_tag(app, tags[i].tag);
                        }
                    }
                }
            }

            /* Content Textarea */
            if (cui_textarea(ui, "tpl_val_in", app->template_value_buf, sizeof app->template_value_buf,
                             .label = "私信正文内容",
                             .placeholder = "在此编写群发私信内容，支持多行文本与上方动态变量标签...",
                             .height = -1, .width = CUI_FILL) & CUI_CHANGED) {
                app->template_changed = true;
            }

            cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                char len_info[64];
                snprintf(len_info, sizeof len_info, "当前字数：%zu 字节（B 站私信单条上限 500 字）", strlen(app->template_value_buf));
                cui_muted(ui, len_info);
                cui_spacer(ui);
                if (cui_button(ui, "清空正文", .size = CUI_SIZE_SM, .variant = CUI_BUTTON_GHOST, .disabled = !app->template_value_buf[0])) {
                    app->template_value_buf[0] = '\0';
                    app->template_changed = true;
                }
            }
        }

        /* Column 3: Multi-Role Live Preview (GROW 5) */
        cui_card(ui, .width = CUI_GROW(5), .height = CUI_FILL, .pad = 16, .gap = 12) {
            cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                cui_icon(ui, CUI_ICON_EYE, 16, CUI_COLOR_ACCENT_STRONG);
                cui_gap(ui, 6);
                cui_label(ui, "实时私信效果预览");
            }

            static const char *preview_modes[] = { "首名", "舰长", "提督", "总督" };
            cui_segmented(ui, "tpl_preview_role", preview_modes, 4, &app->tpl_preview_mode, .size = CUI_SIZE_SM, .full_width = true);

            bm_guard sample_g;
            memset(&sample_g, 0, sizeof sample_g);
            if (app->tpl_preview_mode == 0 && app->store.guards.count > 0) {
                sample_g = app->store.guards.items[0];
            } else if (app->tpl_preview_mode == 3) {
                sample_g.uid = 10001;
                bm_strlcpy(sample_g.username, "示例总督大佬", sizeof sample_g.username);
                sample_g.guard_level = 1;
            } else if (app->tpl_preview_mode == 2) {
                sample_g.uid = 10002;
                bm_strlcpy(sample_g.username, "示例提督伙伴", sizeof sample_g.username);
                sample_g.guard_level = 2;
            } else {
                sample_g.uid = 10003;
                bm_strlcpy(sample_g.username, "示例舰长朋友", sizeof sample_g.username);
                sample_g.guard_level = 3;
            }

            const char *sample_code = app->store.code_count > 0 ? app->store.codes[0].code : "GIFT-2026-ABCD";
            int y, m, d;
            bm_date_today(&y, &m, &d);
            int last_m = (m == 1) ? 12 : (m - 1);
            char date_str[16];
            snprintf(date_str, sizeof date_str, "%04d-%02d-%02d", y, m, d);

            char preview_text[4096];
            bm_template_process(app->template_value_buf, &sample_g, sample_code, date_str, m, last_m, d,
                                preview_text, sizeof preview_text);

            /* Simulated Bilibili Whisper Window */
            cui_box(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 14, .radius = 12,
                    .bg = CUI_COLOR_SURFACE_MUTED, .gap = 10, .scroll_y = true) {
                cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL, .gap = 6) {
                    cui_texture rtex = bm_image_get(ui, sample_g.face, NULL, NULL);
                    cui_avatar(ui, sample_g.username, .size = 26, .image = rtex);
                    bm_guard_badge(ui, &sample_g);
                    cui_text(ui, sample_g.username, .font = CUI_FONT_MEDIUM, .size = 12.5f, .nowrap = true);
                    cui_spacer(ui);
                    cui_copy_button(ui, "copy_preview_msg", preview_text);
                }
                cui_separator(ui);

                const char *sender_name = app->account_profile.uname[0] ? app->account_profile.uname : "当前主播";
                cui_chat_message(ui, preview_text[0] ? preview_text : "（在左侧输入模板内容后，此处将实时渲染私信气泡）",
                                 .mine = true,
                                 .author = sender_name,
                                 .time = date_str,
                                 .status = "预览模式");
            }

            /* Compact Variable Mapping Reference */
            cui_box(ui, .width = CUI_FILL, .pad = 10, .radius = 8, .bg = CUI_COLOR_SURFACE_MUTED, .gap = 4) {
                cui_muted(ui, "当前预览变量取值映射：");
                char map_line1[256], map_line2[256];
                snprintf(map_line1, sizeof map_line1, "{name}=%s  ·  {level}=%s  ·  {code}=%s",
                         sample_g.username, bm_guard_level_str(&sample_g), sample_code);
                snprintf(map_line2, sizeof map_line2, "{date}=%s  ·  {month}=%d  ·  {lastMonth}=%d  ·  {day}=%d",
                         date_str, m, last_m, d);
                cui_secondary(ui, map_line1);
                cui_secondary(ui, map_line2);
            }
        }
    }
}
