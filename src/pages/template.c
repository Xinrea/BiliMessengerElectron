#include "ui/app.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

/* CUI internal layout line helper from cui_textedit.c (linked in target cui) */
typedef struct { int start, end; float width; } bm_cui_line;
extern int cui__edit_lines(cui_ctx *ui, cui_font f, float size, const char *buf, int len, float width, bm_cui_line **out);

typedef struct {
    int cursor, anchor;
    float scroll_x, scroll_y;
} bm_edit_state_head;

typedef struct {
    const char *tag;
    int len;
    int category; /* 0 = User/Guard (Blue), 1 = Gift Code (Violet), 2 = Date/Time (Teal) */
    const char *tip;
} bm_tpl_tag_def;

static const bm_tpl_tag_def k_tpl_tags[7] = {
    { "{name}",      6,  0, "插入目标用户 Bilibili 昵称" },
    { "{level}",     7,  0, "插入大航海等级（总督 / 提督 / 舰长 / 自定义头衔）" },
    { "{code}",      6,  1, "插入并自动消耗一个兑换码" },
    { "{date}",      6,  2, "插入当前完整日期（YYYY-MM-DD）" },
    { "{month}",     7,  2, "插入当前月份数字（1-12）" },
    { "{lastMonth}", 11, 2, "插入上个月份数字（1-12）" },
    { "{day}",       5,  2, "插入当前日期号数（1-31）" }
};

static void bm_tpl_tag_colors(cui_ctx *ui, int category, cui_color *bg, cui_color *border, cui_color *fg) {
    bool dark = (cui_get_theme_mode(ui) == CUI_THEME_DARK);
    if (category == 0) {
        /* User / Guard tags ({name}, {level}) -> Bilibili Blue */
        if (bg)     *bg     = dark ? CUI_RGB(0x162C4E) : CUI_RGB(0xEBF4FF);
        if (border) *border = dark ? CUI_RGB(0x2D5F9E) : CUI_RGB(0x91CAFF);
        if (fg)     *fg     = dark ? CUI_RGB(0x7CC0FF) : CUI_RGB(0x0958D9);
    } else if (category == 1) {
        /* Gift Code tag ({code}) -> Royal Violet */
        if (bg)     *bg     = dark ? CUI_RGB(0x2B1B4E) : CUI_RGB(0xF6EEFF);
        if (border) *border = dark ? CUI_RGB(0x6D3FB3) : CUI_RGB(0xD3ADF7);
        if (fg)     *fg     = dark ? CUI_RGB(0xD8B4FE) : CUI_RGB(0x6F3EC0);
    } else {
        /* Date / Time tags ({date}, {month}, {lastMonth}, {day}) -> Teal */
        if (bg)     *bg     = dark ? CUI_RGB(0x113538) : CUI_RGB(0xE6FFFB);
        if (border) *border = dark ? CUI_RGB(0x236E73) : CUI_RGB(0x87E8DE);
        if (fg)     *fg     = dark ? CUI_RGB(0x5EEAD4) : CUI_RGB(0x0F766E);
    }
}

typedef struct {
    cui_id editor_id;
    cui_id area_id;
    const char *text;
    int text_len;
    bool focused;
} bm_tpl_hl_ctx;

static void paint_tpl_tag_highlights(cui_ctx *ui, cui_rect canvas_rect, void *user) {
    const bm_tpl_hl_ctx *hl = (const bm_tpl_hl_ctx *)user;
    if (!hl || !hl->text || hl->text_len <= 0) return;

    cui_rect r = cui_get_rect(ui, hl->area_id);
    if (r.w <= 0 || r.h <= 0) return;

    const bm_edit_state_head *est = (const bm_edit_state_head *)cui_state(ui, hl->editor_id, 0);
    float scroll_y = est ? est->scroll_y : 0.0f;
    int cursor = est ? est->cursor : 0;
    int anchor = est ? est->anchor : 0;
    int s0 = cursor < anchor ? cursor : anchor;
    int s1 = cursor > anchor ? cursor : anchor;

    cui_theme *th = cui_theme_get(ui);
    float size = th ? th->text_body : 13.0f;
    float line_h = size * (size > 18.0f ? 1.3f : 1.5f);
    cui_text_metrics tm = cui_measure_text(ui, CUI_FONT_BODY, size, "Ag", 2);
    float fh = tm.ascent - tm.descent;

    bm_cui_line *lines = NULL;
    int line_count = cui__edit_lines(ui, CUI_FONT_BODY, size, hl->text, hl->text_len, r.w, &lines);
    if (line_count <= 0 || !lines) return;

    /* Replace the 0x0 canvas clip with the textarea's inner text area clip */
    cui_pop_clip(ui);
    cui_push_clip(ui, r);

    float y0 = r.y - scroll_y;
    for (int k = 0; k < line_count; k++) {
        bm_cui_line *L = &lines[k];
        float ly = y0 + line_h * (float)k;
        if (ly > r.y + r.h || ly + line_h < r.y) continue;
        float ty = ly + (line_h - fh) * 0.5f;
        const char *line = hl->text + L->start;
        int ll = L->end - L->start;

        for (int pos = 0; pos < ll; pos++) {
            if (line[pos] != '{') continue;
            int matched_idx = -1;
            for (int ti = 0; ti < 7; ti++) {
                int tlen = k_tpl_tags[ti].len;
                if (pos + tlen <= ll && memcmp(line + pos, k_tpl_tags[ti].tag, (size_t)tlen) == 0) {
                    matched_idx = ti;
                    break;
                }
            }
            if (matched_idx < 0) continue;

            int tlen = k_tpl_tags[matched_idx].len;
            int abs_a = L->start + pos;
            int abs_b = abs_a + tlen;

            cui_color bg_col, border_col, fg_col;
            bm_tpl_tag_colors(ui, k_tpl_tags[matched_idx].category, &bg_col, &border_col, &fg_col);

            float x0 = r.x + cui_measure_text(ui, CUI_FONT_BODY, size, line, pos).width;
            float x1 = r.x + cui_measure_text(ui, CUI_FONT_BODY, size, line, pos + tlen).width;
            float tw = x1 - x0;

            cui_rect pill_r = { x0, ty - 1.0f, tw, fh + 2.0f };
            cui_draw_rect_border(ui, pill_r, CUI_CORNERS(4.0f), bg_col, border_col, 1.0f);

            /* Preserve selection tint if user selection overlaps this tag */
            if (hl->focused && s1 > s0) {
                int oa = s0 > abs_a ? s0 : abs_a;
                int ob = s1 < abs_b ? s1 : abs_b;
                if (ob > oa) {
                    float sx0 = r.x + cui_measure_text(ui, CUI_FONT_BODY, size, line, oa - L->start).width;
                    float sx1 = r.x + cui_measure_text(ui, CUI_FONT_BODY, size, line, ob - L->start).width;
                    cui_color sel_col = th ? th->colors[CUI_TOKEN_ACCENT] : CUI_RGB(0x2563EB);
                    sel_col.a = 68;
                    cui_rect sr = { sx0, ly, sx1 - sx0, line_h };
                    cui_draw_rect(ui, sr, CUI_CORNERS(2.0f), sel_col);
                }
            }

            cui_draw_text(ui, CUI_FONT_BODY, size, (cui_vec2){ x0, ty }, fg_col, line + pos, tlen);

            /* Redraw caret on top if it sits inside or at the edge of this tag */
            if (hl->focused && s0 == s1 && cursor >= abs_a && cursor <= abs_b) {
                float cx = r.x + cui_measure_text(ui, CUI_FONT_BODY, size, line, cursor - L->start).width;
                cui_color caret_col = th ? th->colors[CUI_TOKEN_FOREGROUND] : CUI_RGB(0x000000);
                cui_rect cr = { cx - 0.5f, ty, 1.5f, fh };
                cui_draw_rect(ui, cr, CUI_CORNERS(0.75f), caret_col);
            }

            pos += tlen - 1;
        }
    }

    cui_pop_clip(ui);
    cui_push_clip(ui, canvas_rect);
}

static bool render_tpl_tag_chip(cui_ctx *ui, int idx) {
    char id_buf[32];
    snprintf(id_buf, sizeof id_buf, "tpl_tag_chip_%d", idx);
    cui_id cid = cui_make_id(ui, id_buf);
    cui_rect r = cui_get_rect(ui, cid);
    cui_vec2 mp = cui_mouse_pos(ui);
    bool hovered = (r.w > 0 && mp.x >= r.x && mp.x < r.x + r.w && mp.y >= r.y && mp.y < r.y + r.h);
    bool clicked = hovered && cui_mouse_clicked(ui, CUI_MOUSE_LEFT);

    cui_color bg, border, fg;
    bm_tpl_tag_colors(ui, k_tpl_tags[idx].category, &bg, &border, &fg);

    cui_row(ui, .id = id_buf, .align = CUI_ALIGN_CENTER, .gap = 3,
            .pad_x = 8, .pad_y = 4, .radius = 6,
            .bg = bg, .border = hovered ? 1.5f : 1.0f, .border_color = border) {
        cui_icon(ui, CUI_ICON_PLUS, 11, fg);
        cui_text(ui, k_tpl_tags[idx].tag, .font = CUI_FONT_MEDIUM, .size = 12.0f, .color = fg, .nowrap = true);
    }
    if (k_tpl_tags[idx].tip) {
        cui_tooltip(ui, k_tpl_tags[idx].tip);
    }
    return clicked;
}

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

static void insert_tag(cui_ctx *ui, bm_app *app, const char *tag) {
    size_t cur = strlen(app->template_value_buf);
    size_t tlen = strlen(tag);
    if (cur + tlen + 1 >= sizeof app->template_value_buf) return;

    cui_id ed_id = cui_make_id(ui, "tpl_val_in");
    bm_edit_state_head *est = (bm_edit_state_head *)cui_state(ui, ed_id, 0);
    int pos = (int)cur;
    if (est && est->cursor > 0 && est->cursor <= (int)cur) {
        pos = est->cursor;
    }
    memmove(app->template_value_buf + pos + tlen, app->template_value_buf + pos, cur - (size_t)pos + 1);
    memcpy(app->template_value_buf + pos, tag, tlen);
    if (est) {
        est->cursor = est->anchor = pos + (int)tlen;
    }
    app->template_changed = true;
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
                    for (int i = 0; i < 7; i++) {
                        if (render_tpl_tag_chip(ui, i)) {
                            insert_tag(ui, app, k_tpl_tags[i].tag);
                        }
                    }
                }
            }

            /* Content Textarea with Real-Time Tag Highlighting */
            cui_box(ui, .width = CUI_FILL, .height = CUI_FILL, .gap = 0) {
                int tf_flags = cui_textarea(ui, "tpl_val_in", app->template_value_buf, sizeof app->template_value_buf,
                                            .label = "私信正文内容",
                                            .placeholder = "在此编写群发私信内容，支持多行文本与上方动态变量标签...",
                                            .height = -1, .width = CUI_FILL);
                if (tf_flags & CUI_CHANGED) {
                    app->template_changed = true;
                }
                cui_id ed_id = cui_last_id(ui);
                bm_tpl_hl_ctx hl_ctx = {
                    .editor_id = ed_id,
                    .area_id = ed_id ^ 0xED17u,
                    .text = app->template_value_buf,
                    .text_len = (int)strlen(app->template_value_buf),
                    .focused = cui_wants_keyboard(ui)
                };
                cui_canvas(ui, "tpl_val_tag_hl", CUI_PX(0), CUI_PX(0), paint_tpl_tag_highlights, &hl_ctx, sizeof hl_ctx);
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
            cui_box(ui, .width = CUI_FILL, .pad = 10, .radius = 8, .bg = CUI_COLOR_SURFACE_MUTED, .gap = 6) {
                cui_muted(ui, "当前预览变量取值");
                for (int i = 0; i < 7; i++) {
                    const bm_tpl_tag_def *tag = &k_tpl_tags[i];
                    char value[4096];
                    bm_template_process(tag->tag, &sample_g, sample_code, date_str, m, last_m, d,
                                        value, sizeof value);
                    cui_color bg, border, fg;
                    bm_tpl_tag_colors(ui, tag->category, &bg, &border, &fg);

                    cui_row(ui, .width = CUI_FILL, .gap = 10, .align = CUI_ALIGN_START) {
                        cui_column(ui, .width = CUI_PX(96)) {
                            cui_row(ui, .pad_x = 6, .pad_y = 2, .radius = 4,
                                    .bg = bg, .border = 1, .border_color = border) {
                                cui_text(ui, tag->tag, .font = CUI_FONT_MEDIUM, .size = 12,
                                         .color = fg, .nowrap = true);
                            }
                        }
                        cui_column(ui, .width = CUI_FILL, .pad_y = 2) {
                            cui_text(ui, value[0] ? value : "（空）", .size = 12, .width = CUI_FILL);
                        }
                    }
                }
            }
        }
    }
}
