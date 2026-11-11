#include "ui/images.h"
#include "core/http.h"
#include "core/jobs.h"
#include "core/util.h"
#include "qrcodegen.h"

#include <stdlib.h>
#include <string.h>

#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wsign-compare"
#pragma GCC diagnostic ignored "-Wimplicit-fallthrough"
#endif
#include "stb_image.h"
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#endif

#define BM_IMAGE_CACHE_CAP 256
#define BM_MAX_CONCURRENT_DOWNLOADS 4

typedef struct {
    char url[512];
    cui_texture tex;
    int w;
    int h;
    bool loading;
    bool failed;
    unsigned char *pending_rgba;
} img_entry;

static img_entry g_cache[BM_IMAGE_CACHE_CAP];
static int g_cache_count = 0;
static int g_active_downloads = 0;

void bm_images_init(void) {
    memset(g_cache, 0, sizeof g_cache);
    g_cache_count = 0;
    g_active_downloads = 0;
}

void bm_images_shutdown(cui_ctx *ui) {
    for (int i = 0; i < g_cache_count; i++) {
        if (g_cache[i].tex && ui) {
            cui_image_destroy(ui, g_cache[i].tex);
        }
        if (g_cache[i].pending_rgba) {
            stbi_image_free(g_cache[i].pending_rgba);
        }
    }
    memset(g_cache, 0, sizeof g_cache);
    g_cache_count = 0;
}

typedef struct {
    int slot;
    char url[512];
    unsigned char *rgba;
    int w;
    int h;
    bool ok;
} img_dl_task;

static void on_image_downloaded(void *data, void *user) {
    (void)user;
    img_dl_task *t = (img_dl_task *)data;
    if (g_active_downloads > 0) g_active_downloads--;

    if (t->slot >= 0 && t->slot < g_cache_count && strcmp(g_cache[t->slot].url, t->url) == 0) {
        g_cache[t->slot].loading = false;
        if (t->ok && t->rgba && t->w > 0 && t->h > 0) {
            g_cache[t->slot].pending_rgba = t->rgba;
            g_cache[t->slot].w = t->w;
            g_cache[t->slot].h = t->h;
            t->rgba = NULL;
        } else {
            g_cache[t->slot].failed = true;
        }
    }
    if (t->rgba) stbi_image_free(t->rgba);
    free(t);
}

static void download_image_worker(void *arg) {
    img_dl_task *t = (img_dl_task *)arg;
    bm_http_resp resp;
    bm_http_req req = {
        .url = t->url,
        .method = "GET",
        .timeout_ms = 15000
    };
    if (bm_http_request(&req, &resp) && resp.data && resp.size > 0 && resp.status == 200) {
        int w = 0, h = 0, comp = 0;
        unsigned char *px = stbi_load_from_memory((const stbi_uc *)resp.data, (int)resp.size, &w, &h, &comp, 4);
        if (px && w > 0 && h > 0) {
            /* Downscale huge images (> 640px) in worker thread to save GPU memory */
            if (w > 640 || h > 640) {
                int nw = w > h ? 640 : (w * 640) / h;
                int nh = h >= w ? 640 : (h * 640) / w;
                if (nw < 1) nw = 1;
                if (nh < 1) nh = 1;
                unsigned char *scaled = (unsigned char *)malloc((size_t)nw * (size_t)nh * 4);
                if (scaled) {
                    for (int y = 0; y < nh; y++) {
                        int sy = (y * h) / nh;
                        for (int x = 0; x < nw; x++) {
                            int sx = (x * w) / nw;
                            memcpy(&scaled[(y * nw + x) * 4], &px[(sy * w + sx) * 4], 4);
                        }
                    }
                    stbi_image_free(px);
                    px = scaled;
                    w = nw;
                    h = nh;
                }
            }
            t->rgba = px;
            t->w = w;
            t->h = h;
            t->ok = true;
        }
    }
    bm_http_resp_free(&resp);
    bm_job_post_main(on_image_downloaded, t);
}

cui_texture bm_image_get(cui_ctx *ui, const char *url, int *out_w, int *out_h) {
    if (out_w) *out_w = 0;
    if (out_h) *out_h = 0;
    if (!ui || !url || strncmp(url, "http", 4) != 0) return 0;

    for (int i = 0; i < g_cache_count; i++) {
        if (strcmp(g_cache[i].url, url) == 0) {
            if (g_cache[i].pending_rgba) {
                g_cache[i].tex = cui_image_create(ui, g_cache[i].w, g_cache[i].h, g_cache[i].pending_rgba);
                stbi_image_free(g_cache[i].pending_rgba);
                g_cache[i].pending_rgba = NULL;
            }
            if (out_w) *out_w = g_cache[i].w;
            if (out_h) *out_h = g_cache[i].h;
            return g_cache[i].tex;
        }
    }

    if (g_cache_count >= BM_IMAGE_CACHE_CAP || g_active_downloads >= BM_MAX_CONCURRENT_DOWNLOADS) {
        return 0;
    }

    int slot = g_cache_count++;
    memset(&g_cache[slot], 0, sizeof g_cache[slot]);
    bm_strlcpy(g_cache[slot].url, url, sizeof g_cache[slot].url);
    g_cache[slot].loading = true;

    img_dl_task *t = (img_dl_task *)calloc(1, sizeof *t);
    if (!t) {
        g_cache[slot].loading = false;
        g_cache[slot].failed = true;
        return 0;
    }
    t->slot = slot;
    bm_strlcpy(t->url, url, sizeof t->url);
    g_active_downloads++;
    if (!bm_job_spawn(download_image_worker, t)) {
        g_active_downloads--;
        g_cache[slot].loading = false;
        g_cache[slot].failed = true;
        free(t);
    }
    return 0;
}

cui_texture bm_qr_create_texture(cui_ctx *ui, const char *text, int *out_px) {
    if (!ui || !text || !text[0]) return 0;
    uint8_t qrcode[qrcodegen_BUFFER_LEN_MAX];
    uint8_t tempBuffer[qrcodegen_BUFFER_LEN_MAX];
    bool ok = qrcodegen_encodeText(
        text,
        tempBuffer,
        qrcode,
        qrcodegen_Ecc_MEDIUM,
        qrcodegen_VERSION_MIN,
        qrcodegen_VERSION_MAX,
        qrcodegen_Mask_AUTO,
        true
    );
    if (!ok) return 0;

    int qr_size = qrcodegen_getSize(qrcode);
    int border = 2;
    int scale = 6;
    int total_modules = qr_size + border * 2;
    int img_px = total_modules * scale;

    uint8_t *rgba = (uint8_t *)malloc((size_t)img_px * (size_t)img_px * 4);
    if (!rgba) return 0;

    for (int y = 0; y < img_px; y++) {
        int my = y / scale - border;
        for (int x = 0; x < img_px; x++) {
            int mx = x / scale - border;
            bool dark = false;
            if (mx >= 0 && mx < qr_size && my >= 0 && my < qr_size) {
                dark = qrcodegen_getModule(qrcode, mx, my);
            }
            uint8_t v = dark ? 0 : 255;
            size_t idx = ((size_t)y * (size_t)img_px + (size_t)x) * 4;
            rgba[idx + 0] = v;
            rgba[idx + 1] = v;
            rgba[idx + 2] = v;
            rgba[idx + 3] = 255;
        }
    }

    cui_texture tex = cui_image_create(ui, img_px, img_px, rgba);
    free(rgba);
    if (out_px) *out_px = img_px;
    return tex;
}
