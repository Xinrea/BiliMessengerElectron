#include "ui/app.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Async Job 1: Verify & fetch logged-in account profile ---- */

typedef struct {
    bm_login_info login;
    bm_user_profile prof;
    bool ok;
    bm_bili_err err;
} account_job_data;

static void on_account_loaded(void *raw, void *user) {
    account_job_data *d = (account_job_data *)raw;
    bm_app *app = (bm_app *)user;
    if (d->ok) {
        app->account_profile = d->prof;
        bm_store_save(&app->store);
    } else if (d->err.code == -101) {
        bm_store_clear_login(&app->store);
        memset(&app->account_profile, 0, sizeof app->account_profile);
        bm_store_save(&app->store);
        bm_toast_push(app, "登录状态已过期，请重新登录", NULL, CUI_TONE_WARNING);
    }
    free(d);
}

static void account_worker(void *raw) {
    account_job_data *d = (account_job_data *)raw;
    uint64_t mid = (uint64_t)strtoull(d->login.DedeUserID, NULL, 10);
    if (mid > 0) {
        d->ok = bm_bili_get_user_info(&d->login, mid, &d->prof, &d->err);
    }
    bm_job_post_main(on_account_loaded, d);
}

void bm_setting_refresh_account(bm_app *app) {
    if (!app || !app->store.login.has_login) return;
    account_job_data *d = (account_job_data *)calloc(1, sizeof *d);
    if (!d) return;
    d->login = app->store.login;
    if (!bm_job_spawn(account_worker, d)) {
        free(d);
    }
}

/* ---- Async Job 2: Startup update check ---- */

typedef struct {
    char tag[64];
    char body[2048];
    char url[512];
    bool has_update;
    bool manual;
} update_job_data;

static void on_update_checked(void *raw, void *user) {
    update_job_data *d = (update_job_data *)raw;
    bm_app *app = (bm_app *)user;
    if (d->has_update) {
        bm_strlcpy(app->update_tag, d->tag, sizeof app->update_tag);
        bm_strlcpy(app->update_body, d->body, sizeof app->update_body);
        bm_strlcpy(app->update_url, d->url, sizeof app->update_url);
        app->update_dialog = true;
    } else if (d->manual) {
        bm_toast_push(app, "当前已是最新版本 (v2.0.0)", NULL, CUI_TONE_SUCCESS);
    }
    free(d);
}

static void update_worker(void *raw) {
    update_job_data *d = (update_job_data *)raw;
    if (bm_bili_check_github_update(d->tag, sizeof d->tag, d->body, sizeof d->body, d->url, sizeof d->url, NULL)) {
        if (d->tag[0] && strcmp(d->tag, "v2.0.0") != 0 && strcmp(d->tag, "v1.5.5") > 0) {
            d->has_update = true;
        }
    }
    bm_job_post_main(on_update_checked, d);
}

static void trigger_manual_update_check(bm_app *app) {
    update_job_data *u = (update_job_data *)calloc(1, sizeof *u);
    if (u) {
        u->manual = true;
        if (bm_job_spawn(update_worker, u)) {
            bm_toast_push(app, "正在检查 GitHub 最新版本...", NULL, CUI_TONE_INFO);
        } else {
            free(u);
        }
    }
}

void bm_setting_startup_checks(bm_app *app) {
    bm_setting_refresh_account(app);
    bm_guard_list_refresh_calendar(app);
    update_job_data *u = (update_job_data *)calloc(1, sizeof *u);
    if (u && !bm_job_spawn(update_worker, u)) {
        free(u);
    }
}

/* ---- Async Job 3: Generate QR Login URL ---- */

typedef struct {
    char url[512];
    char key[128];
    bool ok;
    bm_bili_err err;
} qr_gen_job;

static void on_qr_generated(void *raw, void *user) {
    qr_gen_job *d = (qr_gen_job *)raw;
    bm_app *app = (bm_app *)user;
    if (d->ok) {
        bm_strlcpy(app->qr_url, d->url, sizeof app->qr_url);
        bm_strlcpy(app->qr_key, d->key, sizeof app->qr_key);
        bm_strlcpy(app->qr_status, "请使用哔哩哔哩手机客户端扫描上方二维码", sizeof app->qr_status);
        app->qr_tex_dirty = true;
        app->qr_last_poll_ms = bm_now_ms();
    } else {
        bm_strlcpy(app->qr_status, d->err.message, sizeof app->qr_status);
    }
    free(d);
}

static void qr_gen_worker(void *raw) {
    qr_gen_job *d = (qr_gen_job *)raw;
    d->ok = bm_bili_qr_generate(d->url, sizeof d->url, d->key, sizeof d->key, &d->err);
    bm_job_post_main(on_qr_generated, d);
}

static void start_qr_login(bm_app *app) {
    app->qr_url[0] = '\0';
    app->qr_key[0] = '\0';
    bm_strlcpy(app->qr_status, "正在生成登录二维码...", sizeof app->qr_status);
    qr_gen_job *d = (qr_gen_job *)calloc(1, sizeof *d);
    if (d && !bm_job_spawn(qr_gen_worker, d)) {
        free(d);
    }
}

/* ---- Async Job 4: Poll QR Login ---- */

typedef struct {
    char key[128];
    int status_code;
    char status_msg[256];
    bm_login_info resolved_login;
    bm_user_profile prof;
    bool login_complete;
    bm_bili_err err;
} qr_poll_job;

static void on_qr_polled(void *raw, void *user) {
    qr_poll_job *d = (qr_poll_job *)raw;
    bm_app *app = (bm_app *)user;
    app->qr_polling = false;

    if (strcmp(d->key, app->qr_key) != 0) {
        free(d);
        return;
    }

    if (d->login_complete) {
        app->store.login = d->resolved_login;
        app->account_profile = d->prof;
        app->qr_key[0] = '\0';
        app->login_dialog = false;
        bm_store_save(&app->store);
        bm_toast_push(app, "扫码登录成功", d->prof.uname, CUI_TONE_SUCCESS);
    } else if (d->status_code == 86038) {
        bm_strlcpy(app->qr_status, "二维码已过期，请点击下方按钮刷新", sizeof app->qr_status);
        app->qr_key[0] = '\0';
    } else if (d->status_code == 86090) {
        bm_strlcpy(app->qr_status, "扫描成功！请在手机上点击「确认登录」", sizeof app->qr_status);
    } else if (d->status_code == 86101) {
        bm_strlcpy(app->qr_status, "请使用哔哩哔哩手机客户端扫描上方二维码", sizeof app->qr_status);
    } else if (d->err.message[0]) {
        bm_strlcpy(app->qr_status, d->err.message, sizeof app->qr_status);
    }
    free(d);
}

static void qr_poll_worker(void *raw) {
    qr_poll_job *d = (qr_poll_job *)raw;
    if (bm_bili_qr_poll(d->key, &d->status_code, d->status_msg, sizeof d->status_msg, &d->resolved_login, &d->err)) {
        if (d->status_code == 0 && d->resolved_login.has_login) {
            uint64_t mid = (uint64_t)strtoull(d->resolved_login.DedeUserID, NULL, 10);
            if (bm_bili_get_user_info(&d->resolved_login, mid, &d->prof, &d->err)) {
                d->login_complete = true;
            }
        }
    }
    bm_job_post_main(on_qr_polled, d);
}

/* ---- Async Job 5: Validate and Save Room ID ---- */

typedef struct {
    char room_id[64];
    bm_room_info rinfo;
    bool ok;
    bm_bili_err err;
} save_room_job;

static void on_room_saved(void *raw, void *user) {
    save_room_job *d = (save_room_job *)raw;
    bm_app *app = (bm_app *)user;
    app->room_saving = false;
    if (!d->ok) {
        bm_toast_push(app, "无法保存：无效的直播间号", d->err.message, CUI_TONE_DANGER);
        free(d);
        return;
    }
    bool changed = strcmp(app->store.room_id, d->room_id) != 0;
    bm_strlcpy(app->store.room_id, d->room_id, sizeof app->store.room_id);
    app->room_edited = false;
    app->room_info = d->rinfo;
    if (changed) {
        bm_guard_list_clear(&app->store.guards);
        bm_guard_list_clear(&app->store.original_guards);
        bm_refresh_guard_stats_and_filters(app, true);
        app->summary_loaded = false;
        bm_summary_refresh(app);
        bm_guard_list_refresh_all(app);
    } else {
        bm_store_save(&app->store);
    }
    char msg[128];
    snprintf(msg, sizeof msg, "已绑定直播间 %s (%s)", d->room_id, d->rinfo.anchor_uname);
    bm_toast_push(app, msg, NULL, CUI_TONE_SUCCESS);
    free(d);
}

static void save_room_worker(void *raw) {
    save_room_job *d = (save_room_job *)raw;
    d->ok = bm_bili_get_room_info(d->room_id, &d->rinfo, &d->err);
    bm_job_post_main(on_room_saved, d);
}

static void apply_send_interval(bm_app *app, int v) {
    if (v < 100) v = 100;
    if (v > 60000) v = 60000;
    app->store.send_interval = v;
    snprintf(app->interval_input, sizeof app->interval_input, "%d", v);
    bm_store_save(&app->store);
}

void bm_page_setting(cui_ctx *ui, bm_app *app) {
    /* Refresh QR texture if needed */
    if (app->qr_tex_dirty && app->qr_url[0]) {
        if (app->qr_tex) {
            cui_image_destroy(ui, app->qr_tex);
            app->qr_tex = 0;
        }
        app->qr_tex = bm_qr_create_texture(ui, app->qr_url, NULL);
        app->qr_tex_dirty = false;
    }

    /* Auto-start QR generation when on QR login tab and not logged in */
    if (!app->store.login.has_login && app->login_tab == 0 && !app->qr_key[0] && !app->qr_url[0]) {
        start_qr_login(app);
    }

    /* Poll QR status every 2.5s while visible & not logged in */
    if (!app->store.login.has_login && app->login_tab == 0 && app->qr_key[0]) {
        cui_request_wakeup(ui, 0.5f);
        int64_t now = bm_now_ms();
        if (!app->qr_polling && (now - app->qr_last_poll_ms) >= 2500) {
            app->qr_last_poll_ms = now;
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

    /* Top Page Header */
    cui_row(ui, .width = CUI_FILL, .align = CUI_ALIGN_CENTER, .gap = 10) {
        cui_column(ui, .gap = 2) {
            cui_heading(ui, "系统设置中心", 3);
            cui_secondary(ui, "管理 Bilibili 登录凭据、目标直播间绑定、发送频率及界面主题");
        }
        cui_spacer(ui);
        if (cui_button(ui, "检查版本更新", .icon = CUI_ICON_REFRESH_CW, .size = CUI_SIZE_SM)) {
            trigger_manual_update_check(app);
        }
    }

    /* Main Two-Column Settings Layout */
    cui_row(ui, .width = CUI_FILL, .gap = 16, .align = CUI_ALIGN_STRETCH, .wrap = true) {
        /* Left Column: Account Security Center (GROW 5) */
        cui_card(ui, .width = CUI_GROW(5), .min_width = 360, .pad = 20, .gap = 14) {
            cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                cui_icon(ui, CUI_ICON_SHIELD, 18, CUI_COLOR_ACCENT_STRONG);
                cui_gap(ui, 6);
                cui_heading(ui, "Bilibili 账号中心", 4);
                cui_spacer(ui);
                cui_badge(ui, app->store.login.has_login ? "已登录" : "未登录",
                          .tone = app->store.login.has_login ? CUI_TONE_SUCCESS : CUI_TONE_WARNING, .dot = true);
            }

            cui_separator(ui);

            if (app->store.login.has_login) {
                const char *uname = app->account_profile.uname;
                const char *uface = app->account_profile.face;

                cui_box(ui, .width = CUI_FILL, .pad = 16, .radius = 12, .bg = CUI_COLOR_SURFACE_MUTED, .gap = 12) {
                    cui_row(ui, .gap = 14, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                        cui_texture ftex = bm_image_get(ui, uface, NULL, NULL);
                        cui_avatar(ui, uname[0] ? uname : "B", .size = 56, .image = ftex, .status = "online");
                        cui_column(ui, .gap = 4, .width = CUI_FILL) {
                            cui_text(ui, uname[0] ? uname : app->store.login.DedeUserID,
                                     .font = CUI_FONT_MEDIUM, .size = 17, .color = CUI_COLOR_ACCENT_STRONG);
                            cui_row(ui, .gap = 6, .align = CUI_ALIGN_CENTER) {
                                char uid_txt[64];
                                snprintf(uid_txt, sizeof uid_txt, "UID %s", app->store.login.DedeUserID);
                                cui_code(ui, uid_txt);
                                cui_copy_button(ui, "copy_my_uid", app->store.login.DedeUserID);
                            }
                        }
                    }
                }

                cui_column(ui, .gap = 6, .width = CUI_FILL) {
                    cui_label(ui, "当前凭据状态");
                    cui_row(ui, .gap = 8, .wrap = true) {
                        cui_badge(ui, app->store.login.SESSDATA[0] ? "SESSDATA 已就绪" : "SESSDATA 缺失",
                                  .tone = app->store.login.SESSDATA[0] ? CUI_TONE_SUCCESS : CUI_TONE_DANGER);
                        cui_badge(ui, app->store.login.bili_jct[0] ? "CSRF (bili_jct) 已就绪" : "CSRF 缺失",
                                  .tone = app->store.login.bili_jct[0] ? CUI_TONE_SUCCESS : CUI_TONE_DANGER);
                        cui_badge(ui, app->store.login.buvid3[0] ? "设备指纹已绑定" : "设备指纹待生成",
                                  .tone = CUI_TONE_INFO);
                    }
                }

                cui_spacer(ui);
                cui_separator(ui);

                cui_row(ui, .gap = 8, .width = CUI_FILL) {
                    if (cui_button(ui, "重新校验状态", .icon = CUI_ICON_REFRESH_CW, .size = CUI_SIZE_SM)) {
                        bm_setting_refresh_account(app);
                        bm_toast_push(app, "已发起账号状态校验", NULL, CUI_TONE_INFO);
                    }
                    if (cui_button(ui, "个人空间", .icon = CUI_ICON_EXTERNAL_LINK, .size = CUI_SIZE_SM)) {
                        char url[256];
                        snprintf(url, sizeof url, "https://space.bilibili.com/%s", app->store.login.DedeUserID);
                        cui_app_open_url(url);
                    }
                    cui_spacer(ui);
                    if (cui_confirm_button(ui, "退出登录", .confirm_label = "确认退出", .icon = CUI_ICON_LOG_OUT,
                                           .variant = CUI_BUTTON_DANGER, .size = CUI_SIZE_SM)) {
                        bm_store_clear_login(&app->store);
                        memset(&app->account_profile, 0, sizeof app->account_profile);
                        bm_store_save(&app->store);
                        start_qr_login(app);
                        bm_toast_push(app, "已退出当前账号", NULL, CUI_TONE_INFO);
                    }
                }
            } else {
                static const char *login_tabs[] = { "客户端扫码登录 (推荐)", "Cookie 手动填入" };
                cui_segmented(ui, "setting_login_tabs", login_tabs, 2, &app->login_tab, .full_width = true);

                if (app->login_tab == 0) {
                    cui_column(ui, .width = CUI_FILL, .align = CUI_ALIGN_CENTER, .gap = 12, .pad_y = 8) {
                        if (app->qr_tex) {
                            cui_box(ui, .pad = 10, .bg = CUI_RGB(0xFFFFFF), .radius = 12) {
                                cui_image(ui, app->qr_tex, .width = CUI_PX(188), .height = CUI_PX(188));
                            }
                        } else {
                            cui_box(ui, .width = CUI_PX(208), .height = CUI_PX(208), .bg = CUI_COLOR_SURFACE_MUTED,
                                    .radius = 12, .align = CUI_ALIGN_CENTER, .justify = CUI_JUSTIFY_CENTER) {
                                cui_spinner(ui, 28);
                            }
                        }
                        cui_secondary(ui, app->qr_status[0] ? app->qr_status : "请使用哔哩哔哩手机客户端扫码登录");
                        if (cui_button(ui, "刷新二维码", .icon = CUI_ICON_REFRESH_CW, .size = CUI_SIZE_SM)) {
                            start_qr_login(app);
                        }
                    }
                } else {
                    cui_column(ui, .width = CUI_FILL, .gap = 10) {
                        cui_muted(ui, "从浏览器开发者工具 (Application -> Cookies) 复制以下三个字段：");
                        cui_input(ui, "in_sessdata", app->manual_sessdata, sizeof app->manual_sessdata,
                                  .label = "SESSDATA", .password = true, .placeholder = "填入 SESSDATA 值");
                        cui_input(ui, "in_uid", app->manual_uid, sizeof app->manual_uid,
                                  .label = "DedeUserID", .placeholder = "填入您的数字 UID");
                        cui_input(ui, "in_jct", app->manual_jct, sizeof app->manual_jct,
                                  .label = "bili_jct (CSRF Token)", .password = true, .placeholder = "填入 bili_jct 值");

                        bool can_submit = app->manual_sessdata[0] && app->manual_uid[0] && app->manual_jct[0];
                        if (cui_button(ui, "验证并保存 Cookie 登录", .icon = CUI_ICON_CHECK,
                                       .variant = CUI_BUTTON_PRIMARY, .full_width = true, .disabled = !can_submit)) {
                            app->store.login.has_login = true;
                            bm_strlcpy(app->store.login.SESSDATA, app->manual_sessdata, sizeof app->store.login.SESSDATA);
                            bm_strlcpy(app->store.login.DedeUserID, app->manual_uid, sizeof app->store.login.DedeUserID);
                            bm_strlcpy(app->store.login.bili_jct, app->manual_jct, sizeof app->store.login.bili_jct);
                            snprintf(app->store.login.cookies, sizeof app->store.login.cookies,
                                     "SESSDATA=%s; DedeUserID=%s; bili_jct=%s",
                                     app->manual_sessdata, app->manual_uid, app->manual_jct);
                            bm_store_save(&app->store);
                            bm_setting_refresh_account(app);
                        }
                    }
                }
            }
        }

        /* Right Column: Room Binding, Rate Limit & Appearance (GROW 6) */
        cui_column(ui, .width = CUI_GROW(6), .min_width = 380, .gap = 14) {
            /* Card 1: Live Room Binding */
            cui_card(ui, .width = CUI_FILL, .pad = 18, .gap = 10) {
                cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                    cui_icon(ui, CUI_ICON_RADIO, 17, CUI_COLOR_ACCENT_STRONG);
                    cui_gap(ui, 6);
                    cui_heading(ui, "目标直播间绑定", 4);
                    cui_spacer(ui);
                    if (app->store.room_id[0]) {
                        char rb[64];
                        snprintf(rb, sizeof rb, "当前房间：%s", app->store.room_id);
                        cui_badge(ui, rb, .tone = CUI_TONE_SUCCESS);
                    }
                }

                cui_row(ui, .gap = 10, .align = CUI_ALIGN_END, .width = CUI_FILL) {
                    if (cui_input(ui, "room_id_in", app->room_input, sizeof app->room_input,
                                  .label = "Bilibili 直播间房间号",
                                  .placeholder = "输入直播间短号或真实房间号",
                                  .width = CUI_FILL) & CUI_CHANGED) {
                        app->room_edited = (strcmp(app->room_input, app->store.room_id) != 0);
                    }
                    if (cui_button(ui, "验证并保存", .icon = CUI_ICON_SAVE, .variant = CUI_BUTTON_PRIMARY,
                                   .disabled = !app->room_edited || !app->room_input[0], .loading = app->room_saving)) {
                        save_room_job *d = (save_room_job *)calloc(1, sizeof *d);
                        if (d) {
                            bm_strlcpy(d->room_id, app->room_input, sizeof d->room_id);
                            app->room_saving = true;
                            if (!bm_job_spawn(save_room_worker, d)) {
                                app->room_saving = false;
                                free(d);
                            }
                        }
                    }
                }
                cui_muted(ui, "更换绑定的直播间号时，会自动刷新房间总览数据并重置当前目标列表");
            }

            /* Card 2: Dispatch Rate & Risk Control */
            cui_card(ui, .width = CUI_FILL, .pad = 18, .gap = 10) {
                cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                    cui_icon(ui, CUI_ICON_CLOCK, 17, CUI_COLOR_ACCENT_STRONG);
                    cui_gap(ui, 6);
                    cui_heading(ui, "群发速率与风控间隔", 4);
                    cui_spacer(ui);
                    char cur_ms[32];
                    snprintf(cur_ms, sizeof cur_ms, "%d ms / 条", app->store.send_interval);
                    cui_badge(ui, cur_ms, .tone = app->store.send_interval >= 1000 ? CUI_TONE_SUCCESS : CUI_TONE_WARNING);
                }

                float intv_f = (float)app->store.send_interval;
                if (cui_slider(ui, "每条私信发送间隔 (毫秒)", &intv_f, 200.0f, 5000.0f, .step = 100.0f, .format = "%.0f ms")) {
                    apply_send_interval(app, (int)intv_f);
                }

                cui_row(ui, .gap = 8, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                    cui_muted(ui, "快捷预设：");
                    if (cui_button(ui, "标准 1000ms", .size = CUI_SIZE_SM, .variant = CUI_BUTTON_OUTLINE)) {
                        apply_send_interval(app, 1000);
                    }
                    if (cui_button(ui, "稳健 1500ms", .size = CUI_SIZE_SM, .variant = CUI_BUTTON_OUTLINE)) {
                        apply_send_interval(app, 1500);
                    }
                    if (cui_button(ui, "保守 2500ms", .size = CUI_SIZE_SM, .variant = CUI_BUTTON_OUTLINE)) {
                        apply_send_interval(app, 2500);
                    }
                }
            }

            /* Card 3: Theme & Local Storage */
            cui_card(ui, .width = CUI_FILL, .pad = 18, .gap = 10) {
                cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                    cui_icon(ui, CUI_ICON_PALETTE, 17, CUI_COLOR_ACCENT_STRONG);
                    cui_gap(ui, 6);
                    cui_heading(ui, "界面外观与本地数据", 4);
                }

                cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                    cui_label(ui, "主题配色模式");
                    cui_spacer(ui);
                    static const char *theme_items[] = { "浅色模式", "深色模式" };
                    int t_idx = app->store.dark_mode ? 1 : 0;
                    if (cui_segmented(ui, "setting_theme_seg", theme_items, 2, &t_idx, .size = CUI_SIZE_SM)) {
                        app->store.dark_mode = (t_idx == 1);
                        cui_set_theme(ui, app->store.dark_mode ? CUI_THEME_DARK : CUI_THEME_LIGHT);
                        bm_sync_theme_tokens(ui, app);
                        bm_store_save(&app->store);
                    }
                }

                cui_row(ui, .align = CUI_ALIGN_CENTER, .width = CUI_FILL) {
                    cui_label(ui, "主按钮强调色");
                    cui_spacer(ui);
                    static const char *accent_items[] = { "B站蓝", "琥珀金", "珊瑚粉", "紫罗兰" };
                    if (cui_segmented(ui, "setting_accent_seg", accent_items, 4, &app->accent_idx, .size = CUI_SIZE_SM)) {
                        bm_sync_theme_tokens(ui, app);
                    }
                }

                cui_separator(ui);

                cui_row(ui, .align = CUI_ALIGN_CENTER, .gap = 8, .width = CUI_FILL) {
                    cui_muted(ui, "配置文件：");
                    cui_code(ui, app->store.path);
                    cui_spacer(ui);
                    cui_copy_button(ui, "copy_cfg_path", app->store.path);
                }
            }
        }
    }
}
