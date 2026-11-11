#include "ui/app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void push_sent_msg(bm_app *app, uint64_t uid, const char *username, const char *msg_key, int64_t ts_ms) {
    if (app->sent_msg_count + 1 > app->sent_msg_cap) {
        int next_cap = app->sent_msg_cap ? app->sent_msg_cap * 2 : 32;
        bm_sent_msg *next = (bm_sent_msg *)realloc(app->sent_msgs, (size_t)next_cap * sizeof(bm_sent_msg));
        if (!next) return;
        app->sent_msgs = next;
        app->sent_msg_cap = next_cap;
    }
    bm_sent_msg *m = &app->sent_msgs[app->sent_msg_count++];
    m->uid = uid;
    bm_strlcpy(m->username, username ? username : "", sizeof m->username);
    bm_strlcpy(m->msg_key, msg_key ? msg_key : "", sizeof m->msg_key);
    m->timestamp_ms = ts_ms;
}

static void pop_first_sent_msg(bm_app *app, bm_sent_msg *out) {
    if (app->sent_msg_count <= 0) return;
    if (out) *out = app->sent_msgs[0];
    if (app->sent_msg_count > 1) {
        memmove(&app->sent_msgs[0], &app->sent_msgs[1], (size_t)(app->sent_msg_count - 1) * sizeof(bm_sent_msg));
    }
    app->sent_msg_count--;
}

static const char *current_template_val(const bm_app *app) {
    if (app->msg_template_idx >= 0 && app->msg_template_idx < app->store.template_count) {
        return app->store.templates[app->msg_template_idx].value;
    }
    if (app->msg_template_name[0]) {
        return bm_store_get_template(&app->store, app->msg_template_name);
    }
    return "";
}

static void build_current_message(const bm_app *app, char *out, size_t cap) {
    const char *tmpl = current_template_val(app);
    if (!tmpl[0] || app->store.guards.count == 0) {
        if (cap) out[0] = '\0';
        return;
    }
    const char *code = (app->store.code_count > 0) ? app->store.codes[0].code : "";
    bm_template_process_today(tmpl, &app->store.guards.items[0], code, out, cap);
}

static void schedule_next_send(bm_app *app);

typedef struct {
    bm_login_info login;
    bm_guard target;
    char content[4096];
    bool uses_code;
    int interval_ms;
    char msg_key[64];
    bool ok;
    bm_bili_err err;
} send_step_job;

static void on_send_step_done(void *raw, void *user) {
    send_step_job *j = (send_step_job *)raw;
    bm_app *app = (bm_app *)user;

    app->msg_progress_index++;
    if (app->store.guards.count > 0) {
        bm_guard_list_remove_at(&app->store.guards, 0);
    }

    if (j->ok) {
        push_sent_msg(app, j->target.uid, j->target.username, j->msg_key, bm_now_ms());
        if (j->uses_code) {
            bm_store_pop_first_code(&app->store);
        }
        bm_store_save(&app->store);
    } else {
        bm_guard_list_push(&app->msg_remain_list, &j->target);
        bm_store_save(&app->store);
        char desc[512];
        snprintf(desc, sizeof desc, "向 %s 发送私信时遇到了问题：%s", j->target.username, j->err.message);
        bm_toast_push(app, "私信发送失败", desc, CUI_TONE_DANGER);
    }

    free(j);
    schedule_next_send(app);
}

static void send_step_worker(void *raw) {
    send_step_job *j = (send_step_job *)raw;
    j->ok = bm_bili_send_message(&j->login, j->target.uid, j->content, j->msg_key, sizeof j->msg_key, &j->err);
    if (j->interval_ms > 0) {
        bm_sleep_ms(j->interval_ms);
    }
    bm_job_post_main(on_send_step_done, j);
}

static void schedule_next_send(bm_app *app) {
    if (app->msg_progress_index < app->msg_total_count && app->store.guards.count > 0) {
        send_step_job *j = (send_step_job *)calloc(1, sizeof *j);
        if (!j) {
            app->msg_in_progress = false;
            return;
        }
        j->login = app->store.login;
        j->target = app->store.guards.items[0];
        build_current_message(app, j->content, sizeof j->content);
        j->uses_code = (strstr(current_template_val(app), "{code}") != NULL);
        j->interval_ms = app->store.send_interval > 0 ? app->store.send_interval : 1000;
        if (!bm_job_spawn(send_step_worker, j)) {
            free(j);
            app->msg_in_progress = false;
        }
    } else {
        app->msg_in_progress = false;
        bm_guard_list_copy(&app->store.guards, &app->msg_remain_list);
        bm_store_save(&app->store);
        char desc[128];
        snprintf(desc, sizeof desc, "共有 %d 条发送失败", app->msg_remain_list.count);
        bm_toast_push(app, "私信发送完成", desc, app->msg_remain_list.count == 0 ? CUI_TONE_SUCCESS : CUI_TONE_WARNING);
        bm_guard_list_clear(&app->msg_remain_list);
    }
}

static void schedule_next_recall(bm_app *app, int *fail_count_ptr);

typedef struct {
    bm_login_info login;
    bm_sent_msg msg;
    int interval_ms;
    int fail_count;
    bool expired_before_call;
    bool ok;
    bm_bili_err err;
} recall_step_job;

static void on_recall_step_done(void *raw, void *user) {
    recall_step_job *j = (recall_step_job *)raw;
    bm_app *app = (bm_app *)user;

    int fail_count = j->fail_count;
    if (j->expired_before_call) {
        bm_toast_push(app, "私信撤回失败", "无法撤回超过两分钟的消息", CUI_TONE_DANGER);
        fail_count += 1 + app->sent_msg_count;
        app->sent_msg_count = 0;
    } else if (!j->ok) {
        fail_count++;
        char desc[512];
        snprintf(desc, sizeof desc, "撤回 %s 私信时遇到了问题：%s", j->msg.username, j->err.message);
        bm_toast_push(app, "私信撤回失败", desc, CUI_TONE_DANGER);
        if (j->err.code == 21041) {
            fail_count += app->sent_msg_count;
            app->sent_msg_count = 0;
        }
    }

    free(j);
    schedule_next_recall(app, &fail_count);
}

static void recall_step_worker(void *raw) {
    recall_step_job *j = (recall_step_job *)raw;
    if (!j->expired_before_call) {
        j->ok = bm_bili_recall_message(&j->login, j->msg.uid, j->msg.msg_key, &j->err);
    }
    if (j->interval_ms > 0) {
        bm_sleep_ms(j->interval_ms);
    }
    bm_job_post_main(on_recall_step_done, j);
}

static void schedule_next_recall(bm_app *app, int *fail_count_ptr) {
    int fails = fail_count_ptr ? *fail_count_ptr : 0;
    if (app->sent_msg_count > 0) {
        recall_step_job *j = (recall_step_job *)calloc(1, sizeof *j);
        if (!j) {
            app->msg_in_progress = false;
            return;
        }
        j->login = app->store.login;
        pop_first_sent_msg(app, &j->msg);
        j->interval_ms = app->store.send_interval > 0 ? app->store.send_interval : 1000;
        j->fail_count = fails;
        if (bm_now_ms() - j->msg.timestamp_ms > 120000) {
            j->expired_before_call = true;
        }
        if (!bm_job_spawn(recall_step_worker, j)) {
            free(j);
            app->msg_in_progress = false;
        }
    } else {
        app->msg_in_progress = false;
        char desc[128];
        snprintf(desc, sizeof desc, "共有 %d 条撤回失败", fails);
        bm_toast_push(app, "私信撤回完成", desc, fails == 0 ? CUI_TONE_SUCCESS : CUI_TONE_WARNING);
    }
}

void bm_page_message(cui_ctx *ui, bm_app *app) {
    if (!app->store.login.has_login) {
        bm_render_setting_alert(ui, app);
        return;
    }

    const char *tmpl_val = current_template_val(app);
    bool needs_code = (strstr(tmpl_val, "{code}") != NULL);

    /* Card 1: Template Selection */
    cui_card(ui, .width = CUI_FILL, .gap = 12) {
        cui_heading(ui, "模板选择", 4);

        if (app->store.template_count > 0) {
            const char *names[128];
            int n = app->store.template_count < 128 ? app->store.template_count : 128;
            for (int i = 0; i < n; i++) names[i] = app->store.templates[i].name;
            if (app->msg_template_idx >= n) app->msg_template_idx = 0;
            if (cui_select(ui, "msg_tmpl_sel", names, n, &app->msg_template_idx, .label = "模板")) {
                if (app->msg_template_idx >= 0 && app->msg_template_idx < n) {
                    bm_strlcpy(app->msg_template_name, names[app->msg_template_idx], sizeof app->msg_template_name);
                }
            }
        } else {
            cui_alert(ui, "尚未创建任何模板", .description = "请先前往「模板管理」页面新增并保存私信模板", .tone = CUI_TONE_WARNING);
        }

        char ro_tmpl[4096];
        bm_strlcpy(ro_tmpl, current_template_val(app), sizeof ro_tmpl);
        cui_textarea(ui, "msg_tmpl_ro", ro_tmpl, sizeof ro_tmpl, .label = "模板内容", .read_only = true, .height = 110);

        if (needs_code) {
            cui_badge(ui, "该模板需要使用兑换码", .tone = CUI_TONE_WARNING, .icon = CUI_ICON_GIFT);
        }
    }

    /* Card 2: Sending */
    cui_card(ui, .width = CUI_FILL, .gap = 12) {
        cui_heading(ui, "发送", 4);

        if (!app->msg_in_progress) {
            app->msg_total_count = app->store.guards.count;
        }
        float ratio = app->msg_total_count > 0 ? (float)app->msg_progress_index / (float)app->msg_total_count : 0.0f;
        char prog_label[64];
        snprintf(prog_label, sizeof prog_label, "当前发送进度：%d / %d", app->msg_progress_index, app->msg_total_count);
        cui_progress(ui, ratio, .label = prog_label, .show_value = true, .tone = CUI_TONE_ACCENT);

        cui_row(ui, .gap = 6, .align = CUI_ALIGN_CENTER, .wrap = true) {
            cui_label(ui, "私信目标：");
            int show_n = app->store.guards.count < 5 ? app->store.guards.count : 5;
            if (show_n == 0) {
                cui_muted(ui, "无");
            } else {
                for (int i = 0; i < show_n; i++) {
                    cui_badge(ui, app->store.guards.items[i].username,
                              .tone = (i == 0) ? CUI_TONE_WARNING : CUI_TONE_NEUTRAL, .solid = (i == 0));
                }
                if (app->store.guards.count > 5) {
                    cui_badge(ui, "...", .tone = CUI_TONE_NEUTRAL);
                }
            }
        }

        char msg_preview[4096];
        build_current_message(app, msg_preview, sizeof msg_preview);
        cui_textarea(ui, "msg_content_ro", msg_preview, sizeof msg_preview, .label = "私信内容", .read_only = true, .height = 120);

        const char *err_msg = "将自动移除发送过的舰长和使用过的兑换码";
        bool blocked = false;
        if (needs_code && app->store.code_count < app->store.guards.count) {
            err_msg = "兑换码数量不足，请添加兑换码";
            blocked = true;
        } else if (tmpl_val[0] == '\0') {
            err_msg = "请先选择模板";
            blocked = true;
        } else if (app->store.guards.count == 0 && !app->msg_in_progress) {
            err_msg = "目标列表为空，请先在「目标列表」中添加私信对象";
            blocked = true;
        }

        cui_row(ui, .gap = 12, .align = CUI_ALIGN_CENTER, .wrap = true) {
            if (cui_button(ui, "开始发送", .icon = CUI_ICON_SEND, .variant = CUI_BUTTON_PRIMARY,
                           .disabled = blocked || app->msg_in_progress, .loading = app->msg_in_progress)) {
                app->msg_in_progress = true;
                app->msg_progress_index = 0;
                app->msg_total_count = app->store.guards.count;
                app->sent_msg_count = 0;
                bm_guard_list_clear(&app->msg_remain_list);
                schedule_next_send(app);
            }
            cui_row(ui, .gap = 6, .align = CUI_ALIGN_CENTER) {
                cui_icon(ui, blocked ? CUI_ICON_CIRCLE_ALERT : CUI_ICON_INFO, 16,
                         blocked ? CUI_COLOR_WARNING : CUI_COLOR_TEXT_SECONDARY);
                cui_secondary(ui, err_msg);
            }
        }
    }

    /* Card 3: Recall */
    cui_card(ui, .width = CUI_FILL, .gap = 12) {
        cui_heading(ui, "撤回消息", 4);

        cui_row(ui, .gap = 6, .align = CUI_ALIGN_CENTER, .wrap = true) {
            cui_label(ui, "本次已私信用户：");
            int show_n = app->sent_msg_count < 5 ? app->sent_msg_count : 5;
            if (show_n == 0) {
                cui_muted(ui, "暂无可撤回记录");
            } else {
                for (int i = 0; i < show_n; i++) {
                    cui_badge(ui, app->sent_msgs[i].username,
                              .tone = (i == 0) ? CUI_TONE_WARNING : CUI_TONE_NEUTRAL, .solid = (i == 0));
                }
                if (app->sent_msg_count > 5) {
                    cui_badge(ui, "...", .tone = CUI_TONE_NEUTRAL);
                }
            }
        }

        cui_row(ui, .gap = 10, .align = CUI_ALIGN_CENTER) {
            if (cui_button(ui, "撤回已发消息", .icon = CUI_ICON_UNDO_2, .variant = CUI_BUTTON_DANGER,
                           .disabled = app->sent_msg_count == 0 || app->msg_in_progress,
                           .loading = app->msg_in_progress)) {
                app->msg_in_progress = true;
                int zero = 0;
                schedule_next_recall(app, &zero);
            }
            cui_muted(ui, "注：B 站私信仅支持撤回发送时间在 2 分钟以内的消息");
        }
    }
}
