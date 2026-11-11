#include "ui/app.h"

#include <stdio.h>

void bm_page_about(cui_ctx *ui, bm_app *app) {
    (void)app;
    char title[128];
    snprintf(title, sizeof title, "BiliMessenger %s", BM_VERSION);

    cui_card(ui, .width = CUI_FILL, .gap = 14, .pad = 24) {
        cui_column(ui, .gap = 4) {
            cui_heading(ui, title, 3);
            cui_secondary(ui, "舰长私信助手 @Xinrea · 基于 CUI (C99 Immediate-Mode UI) 原生构建");
        }

        cui_row(ui, .gap = 8, .wrap = true) {
            if (cui_button(ui, "GitHub", .icon = CUI_ICON_EXTERNAL_LINK, .size = CUI_SIZE_SM)) {
                cui_app_open_url("https://github.com/Xinrea");
            }
            if (cui_button(ui, "Bilibili", .icon = CUI_ICON_EXTERNAL_LINK, .size = CUI_SIZE_SM, .variant = CUI_BUTTON_SECONDARY)) {
                cui_app_open_url("https://space.bilibili.com/475210");
            }
            if (cui_button(ui, "Joi Club", .icon = CUI_ICON_GLOBE, .size = CUI_SIZE_SM, .variant = CUI_BUTTON_SECONDARY)) {
                cui_app_open_url("https://joi-club.cn");
            }
            if (cui_button(ui, "CUI 框架", .icon = CUI_ICON_CODE, .size = CUI_SIZE_SM, .variant = CUI_BUTTON_OUTLINE)) {
                cui_app_open_url("https://github.com/yappologistic/CUI");
            }
        }

        cui_separator(ui);

        cui_row(ui, .gap = 10) {
            if (cui_button(ui, "关注 @轴伊Joi_Channel", .icon = CUI_ICON_HEART, .variant = CUI_BUTTON_PRIMARY)) {
                cui_app_open_url("https://space.bilibili.com/61639371/");
            }
        }
    }
}
