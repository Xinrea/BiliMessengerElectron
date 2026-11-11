#include "ui/app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bm_page_template(cui_ctx *ui, bm_app *app) {
    static const char *tags[] = { "level", "name", "code", "date", "month", "lastMonth", "day" };
    enum { TAG_COUNT = sizeof tags / sizeof tags[0] };

    cui_row(ui, .gap = 16, .width = CUI_FILL, .align = CUI_ALIGN_STRETCH, .wrap = true) {
        /* Left Column: Template Editor */
        cui_card(ui, .width = CUI_GROW(1), .min_width = 360, .gap = 14) {
            cui_heading(ui, "模板", 4);

            if (app->store.template_count > 0) {
                const char *names[128];
                int n = app->store.template_count < 128 ? app->store.template_count : 128;
                for (int i = 0; i < n; i++) {
                    names[i] = app->store.templates[i].name;
                }
                if (cui_select(ui, "tmpl_sel", names, n, &app->template_select_idx,
                               .label = "选择已有模板", .placeholder = "选择一个模板...")) {
                    if (app->template_select_idx >= 0 && app->template_select_idx < app->store.template_count) {
                        bm_strlcpy(app->template_name_buf, app->store.templates[app->template_select_idx].name, sizeof app->template_name_buf);
                        bm_strlcpy(app->template_value_buf, app->store.templates[app->template_select_idx].value, sizeof app->template_value_buf);
                        app->template_changed = false;
                    }
                }
            }

            if (cui_input(ui, "tmpl_name", app->template_name_buf, sizeof app->template_name_buf,
                          .label = "模板名", .placeholder = "输入新模板名称或修改现有模板") & CUI_CHANGED) {
                int idx = bm_store_find_template(&app->store, app->template_name_buf);
                if (idx >= 0) {
                    app->template_select_idx = idx;
                    bm_strlcpy(app->template_value_buf, app->store.templates[idx].value, sizeof app->template_value_buf);
                    app->template_changed = false;
                } else {
                    app->template_changed = true;
                }
            }

            if (cui_textarea(ui, "tmpl_val", app->template_value_buf, sizeof app->template_value_buf,
                             .label = "模板内容", .placeholder = "在此编写私信模板内容...", .height = 180) & CUI_CHANGED) {
                app->template_changed = true;
            }

            cui_row(ui, .gap = 10) {
                if (cui_button(ui, "保存", .icon = CUI_ICON_SAVE, .variant = CUI_BUTTON_PRIMARY, .disabled = !app->template_changed)) {
                    if (!app->template_name_buf[0]) {
                        bm_strlcpy(app->template_name_buf, "未命名", sizeof app->template_name_buf);
                    }
                    bm_store_set_template(&app->store, app->template_name_buf, app->template_value_buf);
                    bm_store_save(&app->store);
                    app->template_select_idx = bm_store_find_template(&app->store, app->template_name_buf);
                    if (app->msg_template_idx < 0 && app->store.template_count > 0) {
                        app->msg_template_idx = app->template_select_idx;
                        bm_strlcpy(app->msg_template_name, app->template_name_buf, sizeof app->msg_template_name);
                    }
                    app->template_changed = false;
                    bm_toast_push(app, "模板已保存", app->template_name_buf, CUI_TONE_SUCCESS);
                }
                if (cui_button(ui, "删除", .icon = CUI_ICON_TRASH_2, .variant = CUI_BUTTON_DANGER,
                               .disabled = bm_store_find_template(&app->store, app->template_name_buf) < 0)) {
                    char deleted_name[128];
                    bm_strlcpy(deleted_name, app->template_name_buf, sizeof deleted_name);
                    if (bm_store_delete_template(&app->store, deleted_name)) {
                        bm_store_save(&app->store);
                        app->template_select_idx = -1;
                        app->template_name_buf[0] = '\0';
                        app->template_value_buf[0] = '\0';
                        app->template_changed = false;
                        bm_toast_push(app, "已删除模板", deleted_name, CUI_TONE_INFO);
                    }
                }
            }

            cui_separator(ui);

            cui_label(ui, "快捷插入变量标签");
            cui_row(ui, .gap = 6, .wrap = true) {
                for (int i = 0; i < TAG_COUNT; i++) {
                    char tag_btn[32];
                    snprintf(tag_btn, sizeof tag_btn, "{%s}", tags[i]);
                    if (cui_button(ui, tag_btn, .size = CUI_SIZE_SM, .variant = CUI_BUTTON_SECONDARY)) {
                        size_t cur = strlen(app->template_value_buf);
                        size_t tlen = strlen(tag_btn);
                        if (cur + tlen + 1 < sizeof app->template_value_buf) {
                            memcpy(app->template_value_buf + cur, tag_btn, tlen + 1);
                            app->template_changed = true;
                        }
                    }
                }
            }

            cui_column(ui, .gap = 4) {
                cui_secondary(ui, "可在编写模板内容时手动输入 tag，或是点击以上 tag 按钮快捷添加。");
                cui_secondary(ui, "模板作为消息被发送时，其中包含的 tag 将会被替换为具体内容；替换后的效果可参考右侧预览。");
                cui_muted(ui, "{level}: 总督/提督/舰长/自定义");
                cui_muted(ui, "{name}: 用户名");
                cui_muted(ui, "{code}: 兑换码");
                cui_muted(ui, "{date}: 日期 (YYYY-MM-DD)");
                cui_muted(ui, "{month}: 月份");
                cui_muted(ui, "{lastMonth}: 上个月的月份");
                cui_muted(ui, "{day}: 天");
            }
        }

        /* Right Column: Live Preview */
        cui_card(ui, .width = CUI_GROW(1), .min_width = 320, .gap = 14) {
            cui_heading(ui, "预览", 4);

            bm_guard sample_user;
            memset(&sample_user, 0, sizeof sample_user);
            sample_user.guard_level = 1;
            bm_strlcpy(sample_user.username, "Xinrea", sizeof sample_user.username);

            char preview_buf[4096];
            bm_template_process_today(
                app->template_value_buf,
                &sample_user,
                "https://afdian.net/redeem/some_code_link",
                preview_buf,
                sizeof preview_buf
            );

            cui_row(ui, .gap = 12, .align = CUI_ALIGN_START, .width = CUI_FILL) {
                cui_avatar(ui, "You", .size = 38);
                cui_box(ui, .width = CUI_FILL, .pad = 14, .bg = CUI_COLOR_SURFACE_MUTED, .radius = 12,
                        .border = 1, .border_color = CUI_COLOR_BORDER) {
                    if (preview_buf[0]) {
                        cui_text(ui, preview_buf);
                    } else {
                        cui_muted(ui, "（在左侧输入模板内容以查看实时预览）");
                    }
                }
            }
        }
    }
}
