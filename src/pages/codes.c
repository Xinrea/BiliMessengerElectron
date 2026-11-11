#include "ui/app.h"

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

void bm_page_codes(cui_ctx *ui, bm_app *app) {
    int sel_count = count_selected_codes(&app->store);

    cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .gap = 12) {
        cui_card(ui, .width = CUI_FILL, .pad = 16) {
            cui_row(ui, .gap = 8, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                char count_txt[64];
                snprintf(count_txt, sizeof count_txt, "兑换码总数：%d", app->store.code_count);
                cui_heading(ui, count_txt, 4);
                cui_spacer(ui);

                if (cui_button(ui, "导入", .icon = CUI_ICON_PLUS, .variant = CUI_BUTTON_PRIMARY, .size = CUI_SIZE_SM)) {
                    app->code_import_buf[0] = '\0';
                    app->add_code_dialog = true;
                }

                char del_btn[64];
                snprintf(del_btn, sizeof del_btn, "删除 %d", sel_count);
                if (cui_button(ui, del_btn, .icon = CUI_ICON_TRASH_2, .variant = CUI_BUTTON_DANGER,
                               .size = CUI_SIZE_SM, .disabled = sel_count == 0)) {
                    int removed = bm_store_delete_selected_codes(&app->store);
                    bm_store_save(&app->store);
                    char msg[64];
                    snprintf(msg, sizeof msg, "已删除 %d 个兑换码", removed);
                    bm_toast_push(app, msg, NULL, CUI_TONE_INFO);
                }

                if (cui_button(ui, "清空", .icon = CUI_ICON_TRASH_2, .variant = CUI_BUTTON_OUTLINE,
                               .size = CUI_SIZE_SM, .disabled = app->store.code_count == 0)) {
                    bm_store_clear_codes(&app->store);
                    bm_store_save(&app->store);
                    bm_toast_push(app, "兑换码已清空", NULL, CUI_TONE_INFO);
                }
            }
        }

        cui_card(ui, .width = CUI_FILL, .height = CUI_FILL, .pad = 8) {
            if (app->store.code_count == 0) {
                cui_column(ui, .width = CUI_FILL, .height = CUI_FILL, .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER) {
                    if (cui_empty_state(ui, "暂无兑换码",
                                        .icon = CUI_ICON_GIFT,
                                        .description = "一行一个批量粘贴导入兑换码，私信模板使用 {code} 时将自动按序消耗",
                                        .action = "导入兑换码")) {
                        app->code_import_buf[0] = '\0';
                        app->add_code_dialog = true;
                    }
                }
            } else {
                int first = 0, last = 0;
                cui_virtual_list(ui, "codes_vlist", app->store.code_count, 44.0f, CUI_FILL, &first, &last) {
                    for (int i = first; i < last; i++) {
                        bm_code_item *item = &app->store.codes[i];
                        cui_push_id_int(ui, i);
                        cui_row(ui, .width = CUI_FILL, .height = CUI_PX(44), .pad_x = 12, .gap = 10, .align = CUI_ALIGN_CENTER,
                                .bg = item->selected ? CUI_COLOR_ACCENT_SUBTLE : (cui_color){ 0 }, .radius = 8) {
                            cui_checkbox(ui, "##csel", &item->selected);
                            cui_code(ui, item->code);
                        }
                        cui_pop_id(ui);
                    }
                }
            }
        }
    }

    cui_dialog(ui, "添加兑换码", &app->add_code_dialog,
               .description = "一行一个，空行会被自动移除；爱发电可批量生成并复制粘贴于此", .width = 520) {
        cui_textarea(ui, "code_import_area", app->code_import_buf, sizeof app->code_import_buf,
                     .placeholder = "粘贴兑换码列表（每行一个）...", .height = 220, .autofocus = true);
        cui_dialog_footer(ui) {
            if (cui_button(ui, "取消")) app->add_code_dialog = false;
            if (cui_button(ui, "添加", .variant = CUI_BUTTON_PRIMARY)) {
                int added = bm_store_import_codes(&app->store, app->code_import_buf);
                bm_store_save(&app->store);
                app->add_code_dialog = false;
                char msg[64];
                snprintf(msg, sizeof msg, "新增了 %d 个兑换码", added);
                bm_toast_push(app, msg, NULL, CUI_TONE_SUCCESS);
            }
        }
    }
}
