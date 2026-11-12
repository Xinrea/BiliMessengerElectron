#include "ui/app.h"
#include "core/version.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BM_RELEASES_URL "https://github.com/Xinrea/bili-messenger/releases/"

typedef struct {
    char tag[64];
    char url[512];
    bool has_update;
    bool checked;
    bool manual;
} update_job_data;

static void on_update_checked(void *raw, void *user) {
    update_job_data *d = (update_job_data *)raw;
    bm_app *app = (bm_app *)user;
    app->update_checking = false;
    if (d->has_update) {
        bm_strlcpy(app->update_tag, d->tag, sizeof app->update_tag);
        /* Only open this project's HTTPS releases, even if a response is malformed. */
        const char *url = strncmp(d->url, BM_RELEASES_URL, strlen(BM_RELEASES_URL)) == 0
            ? d->url : BM_RELEASES_URL "latest";
        bm_strlcpy(app->update_url, url, sizeof app->update_url);
        app->update_toast_pending = true;
    } else if (d->manual) {
        if (d->checked) {
            bm_toast_push(app, "当前已是最新版本 (v" BM_VERSION ")", NULL, CUI_TONE_SUCCESS);
        } else {
            bm_toast_push(app, "检查更新失败，请稍后重试", NULL, CUI_TONE_WARNING);
        }
    }
    free(d);
}

static void update_worker(void *raw) {
    update_job_data *d = (update_job_data *)raw;
    if (bm_bili_check_github_update(d->tag, sizeof d->tag, NULL, 0, d->url, sizeof d->url, NULL)) {
        int order = 0;
        d->checked = bm_version_compare(d->tag, BM_VERSION, &order);
        d->has_update = d->checked && order > 0;
    }
    bm_job_post_main(on_update_checked, d);
}

void bm_update_check(bm_app *app, bool manual) {
    if (!app || app->update_checking) return;
    update_job_data *d = (update_job_data *)calloc(1, sizeof *d);
    if (d) {
        d->manual = manual;
        app->update_checking = true;
        if (bm_job_spawn(update_worker, d)) return;
        app->update_checking = false;
        free(d);
    }
    if (manual)
        bm_toast_push(app, "无法启动更新检查，请稍后重试", NULL, CUI_TONE_WARNING);
}

void bm_render_update_toast(cui_ctx *ui, bm_app *app) {
    if (app->update_toast_pending) {
        /* Replace the previous notification so its action cannot use a newer URL. */
        if (app->update_toast_handle)
            cui_toast_dismiss(ui, app->update_toast_handle);
        char title[128];
        snprintf(title, sizeof title, "发现新版本 %s", app->update_tag);
        app->update_toast_handle = cui_toast(ui, title,
            .description = "点击查看发布说明并下载更新",
            .tone = CUI_TONE_INFO, .duration = 12.0f, .action = "查看更新");
        app->update_toast_pending = false;
    }
    if (app->update_toast_handle && cui_toast_action_clicked(ui, app->update_toast_handle)) {
        cui_app_open_url(app->update_url);
        cui_toast_dismiss(ui, app->update_toast_handle);
        app->update_toast_handle = 0;
    }
}
