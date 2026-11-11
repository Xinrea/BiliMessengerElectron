#if !defined(_WIN32)
#define _POSIX_C_SOURCE 200809L
#endif

#include "core/util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#else
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#endif

static int64_t (*g_now_ms_hook)(void *user) = NULL;
static void (*g_sleep_ms_hook)(int ms, void *user) = NULL;
static void *g_time_hook_user = NULL;

void bm_set_time_hooks(int64_t (*now_ms_fn)(void *user), void (*sleep_ms_fn)(int ms, void *user), void *user) {
    g_now_ms_hook = now_ms_fn;
    g_sleep_ms_hook = sleep_ms_fn;
    g_time_hook_user = user;
}

size_t bm_strlcpy(char *dst, const char *src, size_t cap) {
    if (!src) src = "";
    size_t len = strlen(src);
    if (cap > 0) {
        size_t copy = len >= cap ? cap - 1 : len;
        memcpy(dst, src, copy);
        dst[copy] = '\0';
    }
    return len;
}

char *bm_strdup(const char *s) {
    if (!s) s = "";
    size_t len = strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (copy) memcpy(copy, s, len + 1);
    return copy;
}

char *bm_trim(char *s) {
    if (!s) return s;
    while (*s && isspace((unsigned char)*s)) s++;
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[--len] = '\0';
    }
    return s;
}

void bm_str_replace_all(const char *src, const char *needle, const char *repl, char *out, size_t cap) {
    if (!cap) return;
    if (!src) {
        out[0] = '\0';
        return;
    }
    if (!needle || !needle[0]) {
        bm_strlcpy(out, src, cap);
        return;
    }
    if (!repl) repl = "";

    size_t nlen = strlen(needle);
    size_t rlen = strlen(repl);
    size_t pos = 0;
    const char *p = src;

    while (*p && pos + 1 < cap) {
        const char *hit = strstr(p, needle);
        if (!hit) {
            size_t rem = strlen(p);
            if (rem > cap - 1 - pos) rem = cap - 1 - pos;
            memcpy(out + pos, p, rem);
            pos += rem;
            break;
        }
        size_t chunk = (size_t)(hit - p);
        if (chunk > cap - 1 - pos) chunk = cap - 1 - pos;
        memcpy(out + pos, p, chunk);
        pos += chunk;

        size_t rcopy = rlen;
        if (rcopy > cap - 1 - pos) rcopy = cap - 1 - pos;
        memcpy(out + pos, repl, rcopy);
        pos += rcopy;

        p = hit + nlen;
    }
    out[pos] = '\0';
}

static bool is_unreserved(unsigned char c) {
    return (c >= 'A' && c <= 'Z') ||
           (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') ||
           c == '-' || c == '_' || c == '.' || c == '~';
}

void bm_url_encode(const char *src, char *out, size_t cap) {
    static const char hex[] = "0123456789ABCDEF";
    if (!cap) return;
    if (!src) {
        out[0] = '\0';
        return;
    }
    size_t pos = 0;
    for (const unsigned char *p = (const unsigned char *)src; *p && pos + 1 < cap; p++) {
        unsigned char c = *p;
        if (is_unreserved(c)) {
            out[pos++] = (char)c;
        } else {
            if (pos + 3 >= cap) break;
            out[pos++] = '%';
            out[pos++] = hex[(c >> 4) & 0x0F];
            out[pos++] = hex[c & 0x0F];
        }
    }
    out[pos] = '\0';
}

void bm_url_encode_wbi(const char *src, char *out, size_t cap) {
    static const char hex[] = "0123456789ABCDEF";
    if (!cap) return;
    if (!src) {
        out[0] = '\0';
        return;
    }
    size_t pos = 0;
    for (const unsigned char *p = (const unsigned char *)src; *p && pos + 1 < cap; p++) {
        unsigned char c = *p;
        if (c == '!' || c == '\'' || c == '(' || c == ')' || c == '*') {
            continue;
        }
        if (is_unreserved(c)) {
            out[pos++] = (char)c;
        } else {
            if (pos + 3 >= cap) break;
            out[pos++] = '%';
            out[pos++] = hex[(c >> 4) & 0x0F];
            out[pos++] = hex[c & 0x0F];
        }
    }
    out[pos] = '\0';
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

void bm_url_decode(const char *src, char *out, size_t cap) {
    if (!cap) return;
    if (!src) {
        out[0] = '\0';
        return;
    }
    size_t pos = 0;
    for (size_t i = 0; src[i] && pos + 1 < cap; i++) {
        if (src[i] == '%' && src[i + 1] && src[i + 2]) {
            int hi = hex_val(src[i + 1]);
            int lo = hex_val(src[i + 2]);
            if (hi >= 0 && lo >= 0) {
                out[pos++] = (char)((hi << 4) | lo);
                i += 2;
                continue;
            }
        }
        out[pos++] = src[i];
    }
    out[pos] = '\0';
}

void bm_normalize_sessdata(const char *src, char *out, size_t cap) {
    if (!cap) return;
    if (!src) {
        out[0] = '\0';
        return;
    }
    char buf1[512], buf2[512];
    bm_strlcpy(buf1, src, sizeof buf1);
    char *trimmed = bm_trim(buf1);
    memmove(buf1, trimmed, strlen(trimmed) + 1);

    for (int i = 0; i < 2; i++) {
        bm_url_decode(buf1, buf2, sizeof buf2);
        if (strcmp(buf1, buf2) == 0) break;
        bm_strlcpy(buf1, buf2, sizeof buf1);
    }
    bm_strlcpy(out, buf1, cap);
}

void bm_uuid_v4(char *out, size_t cap, bool uppercase) {
    static const char *tmpl = "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx";
    static bool seeded = false;
    if (!seeded) {
        srand((unsigned int)time(NULL) ^ (unsigned int)bm_now_ms());
        seeded = true;
    }
    const char *hex = uppercase ? "0123456789ABCDEF" : "0123456789abcdef";
    size_t pos = 0;
    for (size_t i = 0; tmpl[i] && pos + 1 < cap; i++) {
        char c = tmpl[i];
        if (c == 'x' || c == 'y') {
            int r = rand() & 0x0F;
            int v = (c == 'x') ? r : ((r & 0x03) | 0x08);
            out[pos++] = hex[v];
        } else {
            out[pos++] = c;
        }
    }
    if (cap > 0) out[pos] = '\0';
}

bool bm_is_leap_year(int year) {
    return (year % 4 == 0 && year % 100 != 0) || (year % 400 == 0);
}

int bm_days_in_month(int year, int month) {
    static const int days[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    if (month < 1 || month > 12) return 0;
    if (month == 2 && bm_is_leap_year(year)) return 29;
    return days[month - 1];
}

bool bm_parse_ymd(const char *s, int *year, int *month, int *day) {
    if (!s || strlen(s) != 10) return false;
    for (int i = 0; i < 10; i++) {
        if (i == 4 || i == 7) {
            if (s[i] != '-') return false;
        } else {
            if (!isdigit((unsigned char)s[i])) return false;
        }
    }
    int y = (s[0] - '0') * 1000 + (s[1] - '0') * 100 + (s[2] - '0') * 10 + (s[3] - '0');
    int m = (s[5] - '0') * 10 + (s[6] - '0');
    int d = (s[8] - '0') * 10 + (s[9] - '0');
    int dim = bm_days_in_month(y, m);
    if (y < 1970 || dim == 0 || d < 1 || d > dim) return false;
    if (year) *year = y;
    if (month) *month = m;
    if (day) *day = d;
    return true;
}

void bm_date_today(int *year, int *month, int *day) {
    time_t now = time(NULL);
    struct tm tm_val;
#if defined(_WIN32)
    localtime_s(&tm_val, &now);
#else
    localtime_r(&now, &tm_val);
#endif
    if (year) *year = tm_val.tm_year + 1900;
    if (month) *month = tm_val.tm_mon + 1;
    if (day) *day = tm_val.tm_mday;
}

void bm_date_today_str(char *out, size_t cap) {
    int y, m, d;
    bm_date_today(&y, &m, &d);
    snprintf(out, cap, "%04d-%02d-%02d", y, m, d);
}

void bm_unix_to_ymd(int64_t unix_sec, char *out, size_t cap) {
    time_t t = (time_t)unix_sec;
    struct tm tm_val;
#if defined(_WIN32)
    localtime_s(&tm_val, &t);
#else
    localtime_r(&t, &tm_val);
#endif
    snprintf(out, cap, "%04d-%02d-%02d", tm_val.tm_year + 1900, tm_val.tm_mon + 1, tm_val.tm_mday);
}

void bm_unix_to_mmdd(int64_t unix_sec, char *out, size_t cap) {
    time_t t = (time_t)unix_sec;
    struct tm tm_val;
#if defined(_WIN32)
    localtime_s(&tm_val, &t);
#else
    localtime_r(&t, &tm_val);
#endif
    snprintf(out, cap, "%02d-%02d", tm_val.tm_mon + 1, tm_val.tm_mday);
}

int64_t bm_ymd_to_days(int y, int m, int d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + (unsigned)d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

int64_t bm_now_ms(void) {
    if (g_now_ms_hook) return g_now_ms_hook(g_time_hook_user);
#if defined(_WIN32)
    return (int64_t)GetTickCount64();
#else
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (int64_t)tv.tv_sec * 1000 + (int64_t)(tv.tv_usec / 1000);
#endif
}

void bm_sleep_ms(int ms) {
    if (ms <= 0) return;
    if (g_sleep_ms_hook) {
        g_sleep_ms_hook(ms, g_time_hook_user);
        return;
    }
#if defined(_WIN32)
    Sleep((DWORD)ms);
#else
    struct timespec ts;
    ts.tv_sec = ms / 1000;
    ts.tv_nsec = (long)(ms % 1000) * 1000000L;
    nanosleep(&ts, NULL);
#endif
}

bool bm_ensure_dir(const char *path) {
    if (!path || !path[0]) return false;
    char tmp[1024];
    bm_strlcpy(tmp, path, sizeof tmp);
    size_t len = strlen(tmp);
    if (len > 1 && (tmp[len - 1] == '/' || tmp[len - 1] == '\\')) {
        tmp[len - 1] = '\0';
    }
    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/' || *p == '\\') {
            char saved = *p;
            *p = '\0';
#if defined(_WIN32)
            _mkdir(tmp);
#else
            mkdir(tmp, 0755);
#endif
            *p = saved;
        }
    }
#if defined(_WIN32)
    return _mkdir(tmp) == 0 || errno == EEXIST;
#else
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
#endif
}

bool bm_default_config_path(char *out, size_t cap) {
    char dir[1024] = { 0 };
#if defined(_WIN32)
    const char *appdata = getenv("APPDATA");
    if (!appdata || !appdata[0]) return false;
    snprintf(dir, sizeof dir, "%s\\bilimessenger", appdata);
    bm_ensure_dir(dir);
    snprintf(out, cap, "%s\\config.json", dir);
    return true;
#elif defined(__APPLE__)
    const char *home = getenv("HOME");
    if (!home || !home[0]) return false;
    snprintf(dir, sizeof dir, "%s/Library/Application Support/bilimessenger", home);
    bm_ensure_dir(dir);
    snprintf(out, cap, "%s/config.json", dir);
    return true;
#else
    const char *xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0]) {
        snprintf(dir, sizeof dir, "%s/bilimessenger", xdg);
    } else {
        const char *home = getenv("HOME");
        if (!home || !home[0]) return false;
        snprintf(dir, sizeof dir, "%s/.config/bilimessenger", home);
    }
    bm_ensure_dir(dir);
    snprintf(out, cap, "%s/config.json", dir);
    return true;
#endif
}

bool bm_read_file(const char *path, char **out_buf, size_t *out_len) {
    if (!path || !out_buf) return false;
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return false;
    }
    long sz = ftell(f);
    if (sz < 0) {
        fclose(f);
        return false;
    }
    rewind(f);
    char *buf = (char *)malloc((size_t)sz + 1);
    if (!buf) {
        fclose(f);
        return false;
    }
    size_t n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    buf[n] = '\0';
    *out_buf = buf;
    if (out_len) *out_len = n;
    return true;
}

bool bm_write_file(const char *path, const void *data, size_t len) {
    if (!path) return false;
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    size_t n = len > 0 ? fwrite(data, 1, len, f) : 0;
    fclose(f);
    return n == len;
}
