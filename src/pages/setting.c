#include "ui/app.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    bm_login_info login;
    uint64_t mid;
    bm_user_profile prof;
    bool ok;
} account_job_data;

static void on_account_loaded(void *raw, void *user) {
    account_job_data *d = (account_job_data *)raw;
    bm_app *app = (bm_app *)user;
    if (d->ok && app->store.login.has_login) {
        app->account_profile = d->prof;
    }
    free(d);
}

static void account_worker(void *raw) {
    account_job_data *d = (account_job_data *)raw;
    d->ok = bm_bili_get_user_info(&d->login, d->mid, &d->prof, NULL);
    bm_job_post_main(on_account_loaded, d);
}

void bm_setting_refresh_account(bm_app *app) {
    if (!app || !app->store.login.has_login) return;
    uint64_t mid = (uint64_t)strtoull(app->store.login.DedeUserID, NULL, 10);
    if (mid == 0) return;
    account_job_data *d = (account_job_data *)calloc(1, sizeof *d);
    if (!d) return;
    d->login = app->store.login;
    d->mid = mid;
    if (!bm_job_spawn(account_worker, d)) {
        free(d);
    }
}

typedef struct {
    bm_login_info login;
    bool has_login;
    bool cookie_valid;
    bool cookie_checked;
    bool update_ok;
    char tag[64];
    char body[2048];
    char html_url[512];
} startup_job_data;

static void on_startup_done(void *raw, void *user) {
    startup_job_data *d = (startup_job_data *)raw;
    bm_app *app = (bm_app *)user;

    if (d->cookie_checked && app->store.login.has_login) {
        if (!d->cookie_valid) {
            bm_store_clear_login(&app->store);
            bm_store_save(&app->store);
            memset(&app->account_profile, 0, sizeof app->account_profile);
            bm_toast_push(app, "登录已过期", "请前往设置页重新扫码登录", CUI_TONE_WARNING);
        } else {
            bm_setting_refresh_account(app);
        }
    }

    if (d->update_ok && d->tag[0]) {
        char local_ver[64];
        snprintf(local_ver, sizeof local_ver, "v%s", BM_VERSION);
        if (strcmp(d->tag, local_ver) != 0 && strcmp(d->tag, BM_VERSION) != 0) {
            bm_strlcpy(app->update_tag, d->tag, sizeof app->update_tag);
            bm_strlcpy(app->update_body, d->body, sizeof app->update_body);
            bm_strlcpy(app->update_url, d->html_url, sizeof app->update_url);
            app->update_dialog = true;
        }
    }
    free(d);
}

static void startup_worker(void *raw) {
    startup_job_data *d = (startup_job_data *)raw;
    if (d->has_login) {
        d->cookie_checked = bm_bili_check_cookies(&d->login, &d->cookie_valid, NULL);
    }
    d->update_ok = bm_bili_check_github_update(d->tag, sizeof d->tag, d->body, sizeof d->body, d->html_url, sizeof d->html_url, NULL);
    bm_job_post_main(on_startup_done, d);
}

void bm_setting_startup_checks(bm_app *app) {
    if (!app) return;
    if (app->store.room_id[0]) {
        bm_guard_list_refresh_calendar(app);
    }
    startup_job_data *d = (startup_job_data *)calloc(1, sizeof *d);
    if (!d) return;
    d->login = app->store.login;
    d->has_login = app->store.login.has_login;
    if (!bm_job_spawn(startup_worker, d)) {
        free(d);
    }
}

typedef struct {
    char url[512];
    char key[128];
    bool ok;
    bm_bili_err err;
} qr_gen_job;

static void on_qr_generated(void *raw, void *user) {
    qr_gen_job *j = (qr_gen_job *)raw;
    bm_app *app = (bm_app *)user;
    if (j->ok) {
        bm_strlcpy(app->qr_url, j->url, sizeof app->qr_url);
        bm_strlcpy(app->qr_key, j->key, sizeof app->qr_key);
        bm_strlcpy(app->qr_status, "请使用 bilibili 手机 App 扫描下方二维码", sizeof app->qr_status);
        app->qr_tex_dirty = true;
        app->qr_last_poll_ms = bm_now_ms();
    } else {
        snprintf(app->qr_status, sizeof app->qr_status, "获取二维码失败：%s", j->err.message);
    }
    free(j);
}

static void qr_gen_worker(void *raw) {
    qr_gen_job *j = (qr_gen_job *)raw;
    j->ok = bm_bili_qr_generate(j->url, sizeof j->url, j->key, sizeof j->key, &j->err);
    bm_job_post_main(on_qr_generated, j);
}

static void start_qr_generate(bm_app *app) {
    app->qr_key[0] = '\0';
    app->qr_url[0] = '\0';
    bm_strlcpy(app->qr_status, "正在生成登录二维码...", sizeof app->qr_status);
    qr_gen_job *j = (qr_gen_job *)calloc(1, sizeof *j);
    if (!j) return;
    if (!bm_job_spawn(qr_gen_worker, j)) {
        free(j);
    }
}

typedef struct {
    char key[128];
    int status_code;
    char status_msg[256];
    bm_login_info login;
    bool ok;
    bm_bili_err err;
} qr_poll_job;

static void on_qr_polled(void *raw, void *user) {
    qr_poll_job *j = (qr_poll_job *)raw;
    bm_app *app = (bm_app *)user;
    app->qr_polling = false;
    app->qr_last_poll_ms = bm_now_ms();

    if (strcmp(j->key, app->qr_key) != 0 || !app->login_dialog || app->manual_login_enabled) {
        free(j);
        return;
    }

    if (j->status_msg[0]) {
        bm_strlcpy(app->qr_status, j->status_msg, sizeof app->qr_status);
    }

    if (j->status_code == 0 && j->ok && j->login.has_login) {
        app->store.login = j->login;
        bm_bili_reset_caches();
        bm_store_save(&app->store);
        bm_setting_refresh_account(app);
        app->qr_key[0] = '\0';
        app->room_edited = true;
        app->login_step_scan = false;
        bm_toast_push(app, "登录成功", NULL, CUI_TONE_SUCCESS);
    } else if (j->status_code == 86038) {
        app->qr_key[0] = '\0';
    }
    free(j);
}

static void qr_poll_worker(void *raw) {
    qr_poll_job *j = (qr_poll_job *)raw;
    j->ok = bm_bili_qr_poll(j->key, &j->status_code, j->status_msg, sizeof j->status_msg, &j->login, &j->err);
    bm_job_post_main(on_qr_polled, j);
}

typedef struct {
    char room_id[64];
    bm_room_info rinfo;
    bool ok;
} save_room_job;

static void on_room_saved(void *raw, void *user) {
    save_room_job *j = (save_room_job *)raw;
    bm_app *app = (bm_app *)user;
    app->room_saving = false;
    if (j->ok) {
        bm_strlcpy(app->store.room_id, j->room_id, sizeof app->store.room_id);
        bm_strlcpy(app->room_input, j->room_id, sizeof app->room_input);
        app->room_edited = false;
        bm_store_save(&app->store);
        app->login_dialog = false;
        app->login_step_scan = true;
        app->summary_loaded = false;
        bm_summary_refresh(app);
        bm_guard_list_refresh_all(app);
        bm_toast_push(app, "直播间号保存成功", j->rinfo.title, CUI_TONE_SUCCESS);
    } else {
        bm_toast_push(app, "不存在的直播间，请检查是否填写正确", NULL, CUI_TONE_DANGER);
    }
    free(j);
}

static void save_room_worker(void *raw) {
    save_room_job *j = (save_room_job *)raw;
    j->ok = bm_bili_get_room_info(j->room_id, &j->rinfo, NULL);
    bm_job_post_main(on_room_saved, j);
}

static const char *validate_room_id(const char *s) {
    if (!s || !s[0]) return "必须填写直播间号";
    for (const char *p = s; *p; p++) {
        if (!isdigit((unsigned char)*p)) return "直播间号必须为数字";
    }
    return NULL;
}

static void trigger_save_room(bm_app *app) {
    const char *err = validate_room_id(app->room_input);
    if (err) {
        bm_toast_push(app, err, NULL, CUI_TONE_WARNING);
        return;
    }
    if (app->room_saving) return;
    save_room_job *j = (save_room_job *)calloc(1, sizeof *j);
    if (!j) return;
    bm_strlcpy(j->room_id, app->room_input, sizeof j->room_id);
    app->room_saving = true;
    if (!bm_job_spawn(save_room_worker, j)) {
        app->room_saving = false;
        free(j);
    }
}

void bm_page_setting(cui_ctx *ui, bm_app *app) {
    /* Rebuild QR texture on main thread if needed */
    if (app->qr_tex_dirty) {
        if (app->qr_tex) {
            cui_image_destroy(ui, app->qr_tex);
            app->qr_tex = 0;
        }
        if (app->qr_url[0]) {
            app->qr_tex = bm_qr_create_texture(ui, app->qr_url, NULL);
        }
        app->qr_tex_dirty = false;
    }

    /* Poll QR status every 3s while scan dialog is open */
    if (app->login_dialog && app->login_step_scan && !app->manual_login_enabled && app->qr_key[0]) {
        cui_request_wakeup(ui, 1.0f);
        if (!app->qr_polling && (bm_now_ms() - app->qr_last_poll_ms) >= 3000) {
            qr_poll_job *pj = (qr_poll_job *)calloc(1, sizeof *pj);
            if (pj) {
                bm_strlcpy(pj->key, app->qr_key, sizeof pj->key);
                app->qr_polling = true;
                if (!bm_job_spawn(qr_poll_worker, pj)) {
                    app->qr_polling = false;
                    free(pj);
                }
            }
        }
    }

    /* Card 1: Account Info */
    cui_card(ui, .width = CUI_FILL, .pad = 20) {
        cui_row(ui, .gap = 16, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
            cui_texture face_tex = app->store.login.has_login
                ? bm_image_get(ui, app->account_profile.face, NULL, NULL)
                : 0;
            const char *disp_name = (app->store.login.has_login && app->account_profile.uname[0])
                ? app->account_profile.uname
                : (app->store.login.has_login ? app->store.login.DedeUserID : "未登录");

            cui_avatar(ui, disp_name, .size = 64, .image = face_tex);

            if (app->store.login.has_login) {
                cui_column(ui, .gap = 4) {
                    if (cui_link(ui, disp_name)) {
                        char url[256];
                        snprintf(url, sizeof url, "https://space.bilibili.com/%s", app->store.login.DedeUserID);
                        cui_app_open_url(url);
                    }
                    char uid_txt[96];
                    snprintf(uid_txt, sizeof uid_txt, "UID: %s", app->store.login.DedeUserID);
                    cui_secondary(ui, uid_txt);
                }
            } else {
                cui_column(ui, .gap = 4) {
                    cui_heading(ui, "尚未登录 Bilibili 账号", 4);
                    cui_secondary(ui, "登录后可发送/撤回私信以及查询当前账号收到的上舰记录");
                }
            }

            cui_spacer(ui);

            if (app->store.login.has_login) {
                if (cui_button(ui, "退出登录", .icon = CUI_ICON_LOG_OUT, .variant = CUI_BUTTON_DANGER)) {
                    bm_store_clear_login(&app->store);
                    bm_bili_reset_caches();
                    bm_store_save(&app->store);
                    memset(&app->account_profile, 0, sizeof app->account_profile);
                    bm_toast_push(app, "已退出登录", NULL, CUI_TONE_INFO);
                }
            } else {
                if (cui_button(ui, "请先登录", .icon = CUI_ICON_LOG_IN, .variant = CUI_BUTTON_PRIMARY)) {
                    app->login_dialog = true;
                    app->login_step_scan = true;
                    start_qr_generate(app);
                }
            }
        }
    }

    /* Card 2: Room & Send Interval Settings */
    cui_card(ui, .width = CUI_FILL, .gap = 16, .pad = 20) {
        cui_heading(ui, "应用设置", 4);

        const char *room_err = app->room_edited ? validate_room_id(app->room_input) : NULL;
        cui_row(ui, .gap = 10, .align = CUI_ALIGN_END, .width = CUI_FILL) {
            cui_box(ui, .width = CUI_FILL) {
                int r = cui_input(ui, "room_id_in", app->room_input, sizeof app->room_input,
                                  .label = "直播间号", .placeholder = "请输入数字直播间号", .error = room_err);
                if (r & CUI_CHANGED) app->room_edited = true;
                if ((r & CUI_SUBMITTED) && !room_err) trigger_save_room(app);
            }
            if (cui_button(ui, "保存", .icon = CUI_ICON_SAVE, .variant = CUI_BUTTON_PRIMARY,
                           .disabled = !app->room_edited || room_err != NULL, .loading = app->room_saving)) {
                trigger_save_room(app);
            }
        }

        int iv_flags = cui_input(ui, "send_iv_in", app->interval_input, sizeof app->interval_input,
                                 .label = "发送间隔（毫秒）", .suffix = "ms", .hint = "建议不低于 1000 毫秒以避免触发风控");
        if (iv_flags & (CUI_CHANGED | CUI_BLURRED | CUI_SUBMITTED)) {
            int val = atoi(app->interval_input);
            if (val >= 100) {
                app->store.send_interval = val;
                bm_store_save(&app->store);
            }
        }

        cui_separator(ui);

        bool dark = app->store.dark_mode;
        if (cui_switch(ui, "深色模式", &dark, .description = "切换应用界面的明暗主题配色")) {
            app->store.dark_mode = dark;
            cui_set_theme(ui, dark ? CUI_THEME_DARK : CUI_THEME_LIGHT);
            bm_store_save(&app->store);
        }
    }

    /* Login & Room Setup Dialog */
    const char *dlg_title = app->login_step_scan
        ? (app->manual_login_enabled ? "手动登录" : "扫码登录")
        : "直播间号设置";
    cui_dialog(ui, dlg_title, &app->login_dialog,
               .description = (app->login_step_scan && !app->manual_login_enabled) ? app->qr_status : NULL,
               .width = 460) {
        if (app->login_step_scan) {
            cui_column(ui, .gap = 12, .width = CUI_FILL) {
                cui_checkbox(ui, "手动登录", &app->manual_login_enabled);

                if (!app->manual_login_enabled) {
                    cui_column(ui, .width = CUI_FILL, .align = CUI_ALIGN_CENTER, .gap = 12, .pad_y = 8) {
                        if (app->qr_tex) {
                            cui_image(ui, app->qr_tex, .width = CUI_PX(180), .height = CUI_PX(180), .radius = 8);
                        } else {
                            cui_box(ui, .width = CUI_PX(180), .height = CUI_PX(180), .radius = 8, .bg = CUI_COLOR_SURFACE_MUTED,
                                    .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER) {
                                cui_spinner(ui, 24);
                            }
                        }
                        if (cui_button(ui, "刷新二维码", .icon = CUI_ICON_REFRESH_CW, .size = CUI_SIZE_SM)) {
                            start_qr_generate(app);
                        }
                    }
                } else {
                    if (cui_link(ui, "查看手动获取 Cookie 操作说明")) {
                        cui_app_open_url("https://github.com/Xinrea/BiliMessengerElectron/wiki/%E6%89%8B%E5%8A%A8%E7%99%BB%E5%BD%95%E6%8C%87%E5%8D%97");
                    }
                    cui_input(ui, "man_sess", app->manual_sessdata, sizeof app->manual_sessdata,
                              .label = "SESSDATA", .password = true);
                    cui_input(ui, "man_uid", app->manual_uid, sizeof app->manual_uid,
                              .label = "DedeUserID");
                    cui_input(ui, "man_jct", app->manual_jct, sizeof app->manual_jct,
                              .label = "bili_jct", .password = true);
                    if (cui_button(ui, "确认", .variant = CUI_BUTTON_PRIMARY, .full_width = true)) {
                        memset(&app->store.login, 0, sizeof app->store.login);
                        bm_normalize_sessdata(app->manual_sessdata, app->store.login.SESSDATA, sizeof app->store.login.SESSDATA);
                        bm_strlcpy(app->store.login.DedeUserID, bm_trim(app->manual_uid), sizeof app->store.login.DedeUserID);
                        bm_strlcpy(app->store.login.bili_jct, bm_trim(app->manual_jct), sizeof app->store.login.bili_jct);
                        app->store.login.has_login = bm_login_is_complete(&app->store.login);
                        if (app->store.login.has_login) {
                            bm_bili_reset_caches();
                            bm_store_save(&app->store);
                            bm_setting_refresh_account(app);
                            app->room_edited = true;
                            app->login_step_scan = false;
                            bm_toast_push(app, "手动登录已保存", NULL, CUI_TONE_SUCCESS);
                        } else {
                            bm_toast_push(app, "请完整填写三项登录凭据", NULL, CUI_TONE_WARNING);
                        }
                    }
                }
            }
        } else {
            const char *r_err = validate_room_id(app->room_input);
            cui_column(ui, .gap = 12, .width = CUI_FILL) {
                int r = cui_input(ui, "dlg_room_in", app->room_input, sizeof app->room_input,
                                  .label = "直播间号", .placeholder = "请输入需要管理的直播间号", .error = r_err, .autofocus = true);
                if (r & CUI_CHANGED) app->room_edited = true;
                if ((r & CUI_SUBMITTED) && !r_err) trigger_save_room(app);
            }
            cui_dialog_footer(ui) {
                if (cui_button(ui, "跳过")) {
                    app->login_dialog = false;
                    app->login_step_scan = true;
                }
                if (cui_button(ui, "保存", .variant = CUI_BUTTON_PRIMARY, .disabled = r_err != NULL, .loading = app->room_saving)) {
                    trigger_save_room(app);
                }
            }
        }
    }
}
