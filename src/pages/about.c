#include "ui/app.h"

#include <stdio.h>

void bm_page_about(cui_ctx *ui, bm_app *app) {
    /* Hero Product Card */
    cui_card(ui, .width = CUI_FILL, .pad = 24, .gap = 14) {
        cui_row(ui, .gap = 14, .align = CUI_ALIGN_CENTER, .width = CUI_FILL, .wrap = true) {
            cui_box(ui, .width = CUI_PX(52), .height = CUI_PX(52), .radius = 14, .bg = CUI_COLOR_SURFACE_MUTED,
                    .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER) {
                cui_icon(ui, CUI_ICON_SEND, 26, CUI_COLOR_ACCENT_STRONG);
            }
            cui_column(ui, .gap = 4) {
                cui_row(ui, .gap = 8, .align = CUI_ALIGN_CENTER) {
                    cui_heading(ui, "BiliMessenger 2.0.0", 2);
                    cui_badge(ui, "Native C99", .tone = CUI_TONE_SUCCESS);
                    cui_badge(ui, "CUI v1.3.1", .tone = CUI_TONE_INFO);
                }
                cui_secondary(ui, "Bilibili 直播间大航海舰长私信群发与兑换码发放助手 · 轻量级原生桌面重制版");
            }
            cui_spacer(ui);
            if (cui_button(ui, "关注 @轴伊Joi_Channel", .icon = CUI_ICON_HEART, .variant = CUI_BUTTON_PRIMARY)) {
                cui_app_open_url("https://space.bilibili.com/61639371");
            }
        }

        cui_separator(ui);

        cui_row(ui, .gap = 8, .wrap = true) {
            if (cui_button(ui, "GitHub 仓库", .icon = CUI_ICON_EXTERNAL_LINK, .variant = CUI_BUTTON_OUTLINE, .size = CUI_SIZE_SM)) {
                cui_app_open_url("https://github.com/Xinrea/bili-messenger");
            }
            if (cui_button(ui, "作者 Bilibili (@Xinrea)", .icon = CUI_ICON_EXTERNAL_LINK, .variant = CUI_BUTTON_OUTLINE, .size = CUI_SIZE_SM)) {
                cui_app_open_url("https://space.bilibili.com/475210");
            }
            if (cui_button(ui, "PowerLive 官网", .icon = CUI_ICON_GLOBE, .variant = CUI_BUTTON_OUTLINE, .size = CUI_SIZE_SM)) {
                cui_app_open_url("https://powerlive.io/");
            }
        }
    }

    /* Bottom Two-Column Info Cards */
    cui_row(ui, .width = CUI_FILL, .gap = 16, .align = CUI_ALIGN_STRETCH, .wrap = true) {
        /* Left: Workflow & Shortcuts */
        cui_card(ui, .width = CUI_GROW(6), .min_width = 360, .pad = 20, .gap = 12) {
            cui_row(ui, .align = CUI_ALIGN_CENTER, .gap = 6) {
                cui_icon(ui, CUI_ICON_COMPASS, 17, CUI_COLOR_ACCENT_STRONG);
                cui_heading(ui, "标准群发工作流与快捷操作", 4);
            }
            cui_separator(ui);

            cui_timeline_item(ui, "第一步：绑定账号与直播间",
                              .icon = CUI_ICON_SETTINGS, .tone = CUI_TONE_INFO,
                              .description = "在「系统设置」中使用 B 站手机客户端扫码登录，并绑定需要管理的直播间号。");
            cui_timeline_item(ui, "第二步：生成并整理目标名单",
                              .icon = CUI_ICON_USERS, .tone = CUI_TONE_WARNING,
                              .description = "在「目标列表」中拉取某日舰长快照、拉取区间上舰礼物流水，或拖拽导入 TXT/CSV，并使用一键去重与等级筛选。");
            cui_timeline_item(ui, "第三步：准备私信模板与兑换码",
                              .icon = CUI_ICON_FILE_TEXT, .tone = CUI_TONE_VIOLET,
                              .description = "在「模板管理」中编写带 {name}、{level}、{code} 等变量的模板，并在「兑换码库」中批量粘贴兑换码。");
            cui_timeline_item(ui, "第四步：一键群发与 2 分钟内撤回",
                              .icon = CUI_ICON_SEND, .tone = CUI_TONE_SUCCESS, .last = true,
                              .description = "在「私信群发」中实时监控进度，支持失败自动保留重试及 120 秒内单条/批量撤回。");

            cui_separator(ui);
            cui_row(ui, .align = CUI_ALIGN_CENTER, .gap = 8) {
                cui_muted(ui, "全局命令面板快捷键：");
                cui_kbd(ui, "Cmd K");
                cui_muted(ui, "/");
                cui_kbd(ui, "Ctrl K");
            }
        }

        /* Right: Local Storage & Stats */
        cui_card(ui, .width = CUI_GROW(5), .min_width = 320, .pad = 20, .gap = 12) {
            cui_row(ui, .align = CUI_ALIGN_CENTER, .gap = 6) {
                cui_icon(ui, CUI_ICON_DATABASE, 17, CUI_COLOR_ACCENT_STRONG);
                cui_heading(ui, "本地存储数据统计", 4);
            }
            cui_separator(ui);

            char s1[64], s2[64], s3[64], s4[64], s5[64];
            snprintf(s1, sizeof s1, "%d 人（总督 %d / 提督 %d / 舰长 %d）",
                     app->stats.total, app->stats.type1, app->stats.type2, app->stats.type3);
            snprintf(s2, sizeof s2, "%d 人", app->store.guards.count);
            snprintf(s3, sizeof s3, "%d 个", app->store.template_count);
            snprintf(s4, sizeof s4, "%d 个", app->store.code_count);
            snprintf(s5, sizeof s5, "%d 条", app->sent_msg_count);

            cui_list_item(ui, "原始舰长名单池", .icon = CUI_ICON_USERS, .trailing = s1);
            cui_list_item(ui, "当前筛选待发目标", .icon = CUI_ICON_FILTER, .trailing = s2);
            cui_list_item(ui, "已保存私信模板", .icon = CUI_ICON_FILE_TEXT, .trailing = s3);
            cui_list_item(ui, "可用兑换码库存", .icon = CUI_ICON_GIFT, .trailing = s4);
            cui_list_item(ui, "累计记录已发私信", .icon = CUI_ICON_SEND, .trailing = s5);

            cui_spacer(ui);
            cui_separator(ui);
            cui_muted(ui, "100% 兼容旧版 Electron config.json 数据格式");
        }
    }
}
