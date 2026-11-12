#include "ui/app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    bm_feedback draft;
    bm_feedback_result result;
    char path[1100];
    bool saved;
} feedback_job;

static void feedback_uploading(void *raw, void *user) {
    (void)raw;
    bm_app *app = (bm_app *)user;
    app->feedback_uploading = true;
}

static void feedback_progress(void *user) {
    (void)user;
    bm_job_post_main(feedback_uploading, NULL);
}

static void feedback_done(void *raw, void *user) {
    feedback_job *job = (feedback_job *)raw;
    bm_app *app = (bm_app *)user;
    app->feedback = job->draft;
    app->feedback_result = job->result;
    app->feedback_save_failed = !job->saved;
    app->feedback_busy = app->feedback_uploading = false;
    if (job->result.ok)
        bm_toast_push(app, "反馈已收到", job->draft.receipt, CUI_TONE_SUCCESS);
    free(job);
}

static void feedback_worker(void *raw) {
    feedback_job *job = (feedback_job *)raw;
    bm_feedback_send(&job->draft, &job->result, feedback_progress, NULL);
    bm_diagnostics_event(BM_EVENT_FEEDBACK_RESULT, job->result.ok ? BM_LOG_INFO : BM_LOG_WARNING,
                         (int)job->result.status, 0);
    /* Persist the receipt even if the user leaves the page before the callback. */
    job->saved = bm_feedback_save(&job->draft, job->path);
    bm_job_post_main(feedback_done, job);
}

static bool save_feedback(bm_app *app) {
    app->feedback_save_failed = !bm_feedback_save(&app->feedback, app->feedback_path);
    return !app->feedback_save_failed;
}

static void start_feedback(bm_app *app) {
    if (app->feedback_busy || app->feedback_load_failed) return;
    bm_feedback *d = &app->feedback;
    memset(&app->feedback_result, 0, sizeof app->feedback_result);
    if (!d->submission_id[0]) {
        if (!bm_feedback_validate(d, app->feedback_result.message, sizeof app->feedback_result.message)) return;
        bm_feedback_freeze(d);
    }
    /* Never send an ID that cannot survive a restart. */
    if (!save_feedback(app)) return;
    feedback_job *job = (feedback_job *)calloc(1, sizeof *job);
    if (job) {
        job->draft = *d;
        bm_strlcpy(job->path, app->feedback_path, sizeof job->path);
        app->feedback_busy = true;
        app->feedback_uploading = false;
        if (bm_job_spawn(feedback_worker, job)) return;
        app->feedback_busy = false;
        free(job);
    }
    app->feedback_result.retryable = true;
    bm_strlcpy(app->feedback_result.message, "无法启动提交任务，请稍后重试。", sizeof app->feedback_result.message);
}

static void render_feedback(cui_ctx *ui, bm_app *app) {
    bm_feedback *d = &app->feedback;
    bool frozen = d->submission_id[0] != '\0';
    bool received = d->receipt[0] != '\0';
    bool readonly = frozen || app->feedback_busy || app->feedback_load_failed;
    int64_t remaining = d->retry_at_ms > bm_now_ms() ? (d->retry_at_ms - bm_now_ms()) / 1000 + 1 : 0;
    if (remaining) cui_request_wakeup(ui, 1.0f);

    if (app->feedback_load_failed) {
        cui_alert(ui, "无法读取本地反馈草稿，已停止提交以避免丢失原编号。",
                  .description = "请联系维护者检查下方文件，修复后重启软件。", .tone = CUI_TONE_DANGER);
        cui_code(ui, app->feedback_path);
        return;
    }
    cui_column(ui, .width = CUI_FILL, .gap = 12) {
        cui_muted(ui, "提交至 PowerLive，包含填写的内容、可选邮箱、版本和系统类型。默认附带诊断日志，可预览或取消。");
        cui_muted(ui, "请勿填写 Cookie、密码或其他敏感信息。草稿会保存在本机；邮箱仅用于联系，不会自动发送确认邮件。");

        if (received) {
            cui_alert(ui, "反馈已收到", .description = "收到回执不代表已经处理。感谢您帮助改进 BiliMessenger。", .tone = CUI_TONE_SUCCESS);
            cui_row(ui, .gap = 8, .align = CUI_ALIGN_CENTER, .wrap = true) {
                cui_label(ui, "反馈编号");
                cui_code(ui, d->receipt);
                cui_copy_button(ui, "copy_feedback_receipt", d->receipt);
            }
        } else {
            int changes = cui_textarea(ui, "feedback_message", d->message, sizeof d->message,
                                       .label = "问题描述", .placeholder = "例如：点击发送后出现……\n复现步骤：1. …… 2. ……",
                                       .hint = "1–5000 个字符，支持中文、换行和 emoji",
                                       .height = 160, .width = CUI_FILL, .read_only = readonly);
            changes |= cui_input(ui, "feedback_email", d->email, sizeof d->email,
                                 .label = "联系邮箱（可选）", .placeholder = "user@example.com",
                                 .width = CUI_FILL, .read_only = readonly);
            if (changes & CUI_CHANGED) {
                memset(&app->feedback_result, 0, sizeof app->feedback_result);
                save_feedback(app);
            }
            if (cui_checkbox(ui, "自动附带诊断日志（推荐）", &d->include_log, .disabled = readonly,
                             .description = "包含启动、配置、文件读写、后台任务、业务操作和请求日志；不含账号、正文、路径或 Cookie。")) {
                d->log[0] = '\0';
                app->feedback_log_preview = false;
                save_feedback(app);
            }
            if (d->include_log) {
                cui_row(ui, .gap = 8, .wrap = true, .align = CUI_ALIGN_CENTER) {
                    if (cui_button(ui, "预览日志",
                                   .size = CUI_SIZE_SM, .disabled = app->feedback_busy)) {
                        app->feedback_log_preview = true;
                        if (!d->log[0]) {
                            bm_feedback_capture_log(d);
                            save_feedback(app);
                        }
                    }
                    if (d->log[0]) {
                        char size[96];
                        snprintf(size, sizeof size, "diagnostics.log · %zu 字节 · 提交和重试使用此快照", strlen(d->log));
                        cui_muted(ui, size);
                    } else {
                        cui_muted(ui, "提交时自动生成日志快照，无需选择文件。");
                    }
                    if (!readonly && d->log[0] && cui_button(ui, "刷新日志快照", .size = CUI_SIZE_SM)) {
                        bm_feedback_capture_log(d);
                        save_feedback(app);
                    }
                }
            }
            if (frozen)
                cui_muted(ui, "此反馈内容已固定。重试沿用原编号；如需改写，请选择「修改为新反馈」。");
            if (app->feedback_busy) {
                cui_row(ui, .gap = 8, .align = CUI_ALIGN_CENTER) {
                    cui_spinner(ui, 16);
                    cui_secondary(ui, app->feedback_uploading ? "正在提交反馈…" : "正在准备并确认原反馈…");
                }
            } else if (app->feedback_result.message[0]) {
                cui_alert(ui, app->feedback_result.message, .tone = CUI_TONE_WARNING);
            }
            if (remaining) {
                char wait[96];
                snprintf(wait, sizeof wait, "等待重试：%lld 秒后可再次提交（不会自动重试）", (long long)remaining);
                cui_secondary(ui, wait);
            }
        }
        if (app->feedback_save_failed) {
            cui_alert(ui, "本地草稿或回执保存失败",
                      .description = "请检查磁盘空间和目录权限；已收到的回执请先复制保存。", .tone = CUI_TONE_DANGER);
            if (cui_button(ui, "重新保存到本机", .disabled = app->feedback_busy)) save_feedback(app);
        }
        if (frozen && !received && !app->feedback_busy)
            cui_muted(ui, "原反馈可能已被接收；改写并提交新反馈可能产生重复。");
    }
}

void bm_render_feedback_dialog(cui_ctx *ui, bm_app *app) {
    bool preview = app->feedback_log_preview && app->feedback.include_log;
    cui_dialog(ui, "问题反馈", &app->feedback_dialog, .width = 760,
               .description = preview ? "日志预览：以下是将发送的完整快照，可滚动阅读或复制。"
                                      : "请描述操作步骤、预期结果和实际情况。无需登录。") {
        /* Keep actions visible even when previewing a long log on a small window. */
        float body_height = cui_viewport_size(ui).y - 240;
        if (body_height > 520) body_height = 520;
        if (body_height < 120) body_height = 120;
        if (preview) {
            cui_row(ui, .id = "feedback_log_toolbar", .width = CUI_FILL, .gap = 8, .align = CUI_ALIGN_CENTER) {
                cui_muted(ui, "diagnostics.log");
                cui_spacer(ui);
                cui_label(ui, "复制日志");
                cui_copy_button(ui, "copy_feedback_log", app->feedback.log);
                cui_tooltip(ui, "复制完整日志");
            }
            /* One scroll owner; no textarea inside another clipped scroll container. */
            cui_scroll(ui, .id = "feedback_log_reader", .width = CUI_FILL,
                       .height = CUI_PX(body_height - 52), .pad = 8) {
                cui_column(ui, .id = "feedback_log_text", .width = CUI_FILL) {
                    cui_text(ui, app->feedback.log, .font = CUI_FONT_MONO, .size = 12, .width = CUI_FILL);
                }
            }
        } else {
            cui_scroll(ui, .id = "feedback_form", .width = CUI_FILL,
                       .height = CUI_PX(body_height), .gap = 12) {
                render_feedback(ui, app);
            }
        }
        cui_dialog_footer(ui) {
            bm_feedback *d = &app->feedback;
            bool frozen = d->submission_id[0] != '\0';
            bool received = d->receipt[0] != '\0';
            if (cui_button(ui, app->feedback_busy ? "关闭（继续提交）" : "关闭")) {
                app->feedback_dialog = false;
            }
            if (preview) {
                if (cui_button(ui, "返回反馈", .variant = CUI_BUTTON_PRIMARY))
                    app->feedback_log_preview = false;
            }
            if (!preview && !app->feedback_load_failed && !received) {
                bool stopped = app->feedback_result.code[0] && !app->feedback_result.retryable;
                if (cui_button(ui, frozen ? "重试原反馈" : "提交反馈", .icon = CUI_ICON_SEND,
                               .variant = CUI_BUTTON_PRIMARY, .loading = app->feedback_busy,
                               .disabled = app->feedback_busy || d->retry_at_ms > bm_now_ms() || stopped || !d->message[0])) {
                    start_feedback(app);
                }
            }
            if (!preview && !app->feedback_load_failed && frozen && !app->feedback_busy) {
                if (cui_confirm_button(ui, received ? "再写一条反馈" : "修改为新反馈",
                                       .confirm_label = "确认新建反馈", .variant = CUI_BUTTON_OUTLINE)) {
                    bm_feedback_edit(d);
                    if (received) d->message[0] = d->email[0] = '\0';
                    app->feedback_log_preview = false;
                    memset(&app->feedback_result, 0, sizeof app->feedback_result);
                    save_feedback(app);
                }
            }
        }
    }
}

void bm_page_about(cui_ctx *ui, bm_app *app) {
    /* Hero Product Card */
    cui_card(ui, .width = CUI_FILL, .pad = 24, .gap = 14) {
        cui_row(ui, .gap = 14, .align = CUI_ALIGN_CENTER, .width = CUI_FILL, .wrap = true) {
            cui_box(ui, .width = CUI_PX(52), .height = CUI_PX(52), .radius = 14, .bg = CUI_COLOR_SURFACE_MUTED,
                    .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER) {
                cui_icon(ui, CUI_ICON_SEND, 26, CUI_COLOR_ACCENT_STRONG);
            }
            cui_column(ui, .gap = 4) {
                cui_heading(ui, "BiliMessenger " BM_VERSION, 2);
                cui_secondary(ui, "Bilibili 直播间大航海舰长私信群发与兑换码发放助手");
            }
            cui_spacer(ui);
            if (cui_button(ui, "关注 @轴伊Joi_Channel", .icon = CUI_ICON_HEART, .variant = CUI_BUTTON_PRIMARY)) {
                cui_app_open_url("https://space.bilibili.com/61639371");
            }
        }

        cui_separator(ui);

        cui_row(ui, .gap = 8, .wrap = true) {
            if (cui_button(ui, "检查更新", .icon = CUI_ICON_REFRESH_CW,
                           .variant = CUI_BUTTON_OUTLINE, .size = CUI_SIZE_SM,
                           .loading = app->update_checking, .disabled = app->update_checking)) {
                bm_update_check(app, true);
            }
            if (cui_button(ui, "GitHub 仓库", .icon = CUI_ICON_EXTERNAL_LINK, .variant = CUI_BUTTON_OUTLINE, .size = CUI_SIZE_SM)) {
                cui_app_open_url("https://github.com/Xinrea/bili-messenger");
            }
            if (cui_button(ui, "作者 Bilibili (@Xinrea)", .icon = CUI_ICON_EXTERNAL_LINK, .variant = CUI_BUTTON_OUTLINE, .size = CUI_SIZE_SM)) {
                cui_app_open_url("https://space.bilibili.com/475210");
            }
            if (cui_button(ui, "PowerLive 官网", .icon = CUI_ICON_GLOBE, .variant = CUI_BUTTON_OUTLINE, .size = CUI_SIZE_SM)) {
                cui_app_open_url("https://powerlive.io/");
            }
            if (cui_button(ui, "问题反馈", .icon = CUI_ICON_SEND, .variant = CUI_BUTTON_OUTLINE, .size = CUI_SIZE_SM)) {
                app->feedback_dialog = true;
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
        }
    }
}
