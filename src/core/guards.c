#include "core/guards.h"
#include "core/util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bm_guard_list_init(bm_guard_list *list) {
    memset(list, 0, sizeof *list);
}

void bm_guard_list_free(bm_guard_list *list) {
    if (!list) return;
    free(list->items);
    list->items = NULL;
    list->count = 0;
    list->cap = 0;
}

void bm_guard_list_clear(bm_guard_list *list) {
    if (!list) return;
    list->count = 0;
}

static bool ensure_cap(bm_guard_list *list, int needed) {
    if (needed <= list->cap) return true;
    int next_cap = list->cap ? list->cap * 2 : 32;
    while (next_cap < needed) next_cap *= 2;
    bm_guard *next = (bm_guard *)realloc(list->items, (size_t)next_cap * sizeof(bm_guard));
    if (!next) return false;
    list->items = next;
    list->cap = next_cap;
    return true;
}

bool bm_guard_list_push(bm_guard_list *list, const bm_guard *g) {
    if (!ensure_cap(list, list->count + 1)) return false;
    list->items[list->count++] = *g;
    return true;
}

bool bm_guard_list_unshift(bm_guard_list *list, const bm_guard *g) {
    if (!ensure_cap(list, list->count + 1)) return false;
    if (list->count > 0) {
        memmove(&list->items[1], &list->items[0], (size_t)list->count * sizeof(bm_guard));
    }
    list->items[0] = *g;
    list->count++;
    return true;
}

bool bm_guard_list_copy(bm_guard_list *dst, const bm_guard_list *src) {
    bm_guard_list_clear(dst);
    if (!src || src->count == 0) return true;
    if (!ensure_cap(dst, src->count)) return false;
    memcpy(dst->items, src->items, (size_t)src->count * sizeof(bm_guard));
    dst->count = src->count;
    return true;
}

int bm_guard_list_find_uid(const bm_guard_list *list, uint64_t uid) {
    if (!list) return -1;
    for (int i = 0; i < list->count; i++) {
        if (list->items[i].uid == uid) return i;
    }
    return -1;
}

bool bm_guard_list_remove_at(bm_guard_list *list, int index) {
    if (!list || index < 0 || index >= list->count) return false;
    if (index + 1 < list->count) {
        memmove(&list->items[index], &list->items[index + 1], (size_t)(list->count - index - 1) * sizeof(bm_guard));
    }
    list->count--;
    return true;
}

int bm_guard_list_remove_uid(bm_guard_list *list, uint64_t uid) {
    if (!list) return 0;
    int w = 0;
    for (int r = 0; r < list->count; r++) {
        if (list->items[r].uid != uid) {
            if (w != r) list->items[w] = list->items[r];
            w++;
        }
    }
    int removed = list->count - w;
    list->count = w;
    return removed;
}

void bm_guard_note_send_result(
    bm_guard_list *roster,
    bm_guard_list *queue,
    const bm_guard *target,
    bool delivered
) {
    if (!target) return;
    uint64_t uid = target->uid;
    if (uid == 0) {
        /* Invalid records have no identity: advance one entry, keep the roster. */
        bm_guard_list_remove_at(queue, bm_guard_list_find_uid(queue, 0));
        return;
    }
    if (delivered) bm_guard_list_remove_uid(roster, uid);
    bm_guard_list_remove_uid(queue, uid);
}

const char *bm_guard_level_str(const bm_guard *g) {
    if (!g) return "自定义";
    if (g->custom_title[0]) return g->custom_title;
    switch (g->guard_level) {
        case 1: return "总督";
        case 2: return "提督";
        case 3: return "舰长";
        default: return "自定义";
    }
}

int bm_gift_name_to_level(const char *gift_name) {
    if (!gift_name) return 3;
    if (strcmp(gift_name, "总督") == 0) return 1;
    if (strcmp(gift_name, "提督") == 0) return 2;
    if (strcmp(gift_name, "舰长") == 0) return 3;
    return 3;
}

void bm_guard_set_level_from_tag(bm_guard *g, const char *tag) {
    if (!g) return;
    g->custom_title[0] = '\0';
    if (!tag || !tag[0]) {
        g->guard_level = 0;
        bm_strlcpy(g->custom_title, "自定义", sizeof g->custom_title);
        return;
    }
    if (strcmp(tag, "总督") == 0) {
        g->guard_level = 1;
    } else if (strcmp(tag, "提督") == 0) {
        g->guard_level = 2;
    } else if (strcmp(tag, "舰长") == 0) {
        g->guard_level = 3;
    } else {
        g->guard_level = 0;
        bm_strlcpy(g->custom_title, tag, sizeof g->custom_title);
    }
}

void bm_guard_compute_stats(const bm_guard_list *list, bm_guard_stats *stats) {
    memset(stats, 0, sizeof *stats);
    if (!list) return;
    for (int i = 0; i < list->count; i++) {
        const bm_guard *g = &list->items[i];
        stats->total++;
        if (g->custom_title[0] == '\0') {
            if (g->guard_level == 1) stats->type1++;
            else if (g->guard_level == 2) stats->type2++;
            else if (g->guard_level == 3) stats->type3++;
        }
    }
}

static bool matches_filters(const bm_guard *g, const bm_guard_filters *filters) {
    if (g->custom_title[0] == '\0') {
        if (g->guard_level == 1) return filters->type1;
        if (g->guard_level == 2) return filters->type2;
        if (g->guard_level == 3) return filters->type3;
    }
    return true;
}

void bm_guard_apply_filters(const bm_guard_list *src, const bm_guard_filters *filters, bm_guard_list *dst) {
    bm_guard_list_clear(dst);
    if (!src || !filters) return;
    for (int i = 0; i < src->count; i++) {
        const bm_guard *g = &src->items[i];
        if (matches_filters(g, filters)) {
            bm_guard_list_push(dst, g);
        }
    }
}

void bm_guard_prune_queue(const bm_guard_list *roster, const bm_guard_filters *filters, bm_guard_list *queue) {
    int w = 0;
    for (int r = 0; r < queue->count; r++) {
        const bm_guard *g = &queue->items[r];
        if (!matches_filters(g, filters)) continue;
        /* Gift history can contain multiple tiers for one UID. The first
           roster occurrence does not represent every queued record. */
        bool eligible = false;
        for (int i = 0; i < roster->count; i++) {
            if (roster->items[i].uid == g->uid && matches_filters(&roster->items[i], filters)) {
                eligible = true;
                break;
            }
        }
        if (!eligible) continue;
        if (w != r) queue->items[w] = queue->items[r];
        w++;
    }
    queue->count = w;
}

static int effective_level_rank(const bm_guard *g) {
    if (g->custom_title[0] == '\0' && g->guard_level >= 1 && g->guard_level <= 3) {
        return g->guard_level;
    }
    return 4;
}

int bm_guard_deduplicate(bm_guard_list *list) {
    if (!list || list->count <= 1) return 0;
    int before = list->count;
    int w = 0;
    for (int r = 0; r < list->count; r++) {
        const bm_guard *cand = &list->items[r];
        int existing = -1;
        for (int j = 0; j < w; j++) {
            if (list->items[j].uid == cand->uid) {
                existing = j;
                break;
            }
        }
        if (existing < 0) {
            if (w != r) list->items[w] = *cand;
            w++;
        } else {
            if (effective_level_rank(cand) < effective_level_rank(&list->items[existing])) {
                list->items[existing] = *cand;
            }
        }
    }
    list->count = w;
    return before - w;
}

static void replace_space_with_T(const char *src, char *dst, size_t cap) {
    bm_strlcpy(dst, src, cap);
    for (char *p = dst; *p; p++) {
        if (*p == ' ') *p = 'T';
    }
}

static void replace_T_with_space(const char *src, char *dst, size_t cap) {
    bm_strlcpy(dst, src, cap);
    for (char *p = dst; *p; p++) {
        if (*p == 'T') *p = ' ';
    }
}

char *bm_guard_format_txt(const bm_guard_list *list, size_t *out_len) {
    int count = list ? list->count : 0;
    size_t cap = (size_t)(count + 1) * 256 + 64;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    size_t pos = (size_t)snprintf(buf, cap, "舰队总数：%d\n", count);
    for (int i = 0; i < count; i++) {
        const bm_guard *g = &list->items[i];
        const char *lvl = bm_guard_level_str(g);
        int n;
        if (g->time[0]) {
            char tbuf[64];
            replace_space_with_T(g->time, tbuf, sizeof tbuf);
            n = snprintf(buf + pos, cap - pos, "%s %llu %s %s\n", lvl, (unsigned long long)g->uid, g->username, tbuf);
        } else {
            n = snprintf(buf + pos, cap - pos, "%s %llu %s\n", lvl, (unsigned long long)g->uid, g->username);
        }
        if (n > 0 && pos + (size_t)n < cap) pos += (size_t)n;
    }
    buf[pos] = '\0';
    if (out_len) *out_len = pos;
    return buf;
}

char *bm_guard_format_csv(const bm_guard_list *list, size_t *out_len) {
    int count = list ? list->count : 0;
    size_t cap = (size_t)(count + 1) * 256 + 16;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    /* UTF-8 BOM \xEF\xBB\xBF */
    memcpy(buf, "\xEF\xBB\xBF", 3);
    size_t pos = 3;
    for (int i = 0; i < count; i++) {
        const bm_guard *g = &list->items[i];
        const char *lvl = bm_guard_level_str(g);
        int n;
        if (g->time[0]) {
            char tbuf[64];
            replace_space_with_T(g->time, tbuf, sizeof tbuf);
            n = snprintf(buf + pos, cap - pos, "%s,%llu,%s,%s\n", lvl, (unsigned long long)g->uid, g->username, tbuf);
        } else {
            n = snprintf(buf + pos, cap - pos, "%s,%llu,%s\n", lvl, (unsigned long long)g->uid, g->username);
        }
        if (n > 0 && pos + (size_t)n < cap) pos += (size_t)n;
    }
    buf[pos] = '\0';
    if (out_len) *out_len = pos;
    return buf;
}

bool bm_guard_export_txt(const bm_guard_list *list, const char *path) {
    size_t len = 0;
    char *txt = bm_guard_format_txt(list, &len);
    if (!txt) return false;
    bool ok = bm_write_file(path, txt, len);
    free(txt);
    return ok;
}

bool bm_guard_export_csv(const bm_guard_list *list, const char *path) {
    size_t len = 0;
    char *csv = bm_guard_format_csv(list, &len);
    if (!csv) return false;
    bool ok = bm_write_file(path, csv, len);
    free(csv);
    return ok;
}

bool bm_guard_import_text(bm_guard_list *list, const char *content, bool is_txt) {
    if (!list || !content) return false;
    bm_guard_list_clear(list);

    char sep = is_txt ? ' ' : ',';
    const char *p = content;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t line_len = nl ? (size_t)(nl - p) : strlen(p);
        char line[1024];
        if (line_len >= sizeof line) line_len = sizeof line - 1;
        memcpy(line, p, line_len);
        line[line_len] = '\0';
        if (line_len > 0 && line[line_len - 1] == '\r') {
            line[--line_len] = '\0';
        }
        p = nl ? nl + 1 : p + line_len;

        if (!line[0]) continue;

        char *fields[4] = { NULL, NULL, NULL, NULL };
        int nf = 0;
        char *cur = line;
        while (nf < 4 && cur) {
            fields[nf++] = cur;
            char *next = strchr(cur, sep);
            if (next) {
                *next = '\0';
                cur = next + 1;
            } else {
                cur = NULL;
            }
        }
        if (nf < 3) continue;

        const char *tag = fields[0];
        if ((unsigned char)tag[0] == 0xEF && (unsigned char)tag[1] == 0xBB && (unsigned char)tag[2] == 0xBF) {
            tag += 3;
        }
        uint64_t uid = (uint64_t)strtoull(fields[1], NULL, 10);
        if (uid == 0) continue;

        bm_guard g;
        memset(&g, 0, sizeof g);
        bm_strlcpy(g.face, "static/noface.jpg", sizeof g.face);
        bm_strlcpy(g.username, fields[2], sizeof g.username);
        bm_guard_set_level_from_tag(&g, tag);
        g.is_alive = 0;
        g.rank = 1;
        g.ruid = 61639371;
        g.uid = uid;
        if (nf > 3 && fields[3] && fields[3][0]) {
            replace_T_with_space(fields[3], g.time, sizeof g.time);
        }
        bm_guard_list_push(list, &g);
    }
    return true;
}

bool bm_guard_import_file(bm_guard_list *list, const char *path) {
    if (!list || !path || !path[0]) return false;
    char *buf = NULL;
    size_t len = 0;
    if (!bm_read_file(path, &buf, &len)) return false;
    size_t plen = strlen(path);
    bool is_txt = (plen > 0 && (path[plen - 1] == 't' || path[plen - 1] == 'T'));
    bool ok = bm_guard_import_text(list, buf, is_txt);
    free(buf);
    return ok;
}

void bm_template_process(
    const char *tmpl,
    const bm_guard *user,
    const char *code,
    const char *full_date,
    int month,
    int last_month,
    int day,
    char *out,
    size_t cap
) {
    if (!cap) return;
    if (!tmpl || !tmpl[0] || !user) {
        out[0] = '\0';
        return;
    }
    char b1[4096], b2[4096];
    char mon_str[16], lmon_str[16], day_str[16];
    snprintf(mon_str, sizeof mon_str, "%d", month);
    snprintf(lmon_str, sizeof lmon_str, "%d", last_month);
    snprintf(day_str, sizeof day_str, "%d", day);

    bm_str_replace_all(tmpl, "{level}", bm_guard_level_str(user), b1, sizeof b1);
    bm_str_replace_all(b1, "{name}", user->username, b2, sizeof b2);
    bm_str_replace_all(b2, "{code}", code ? code : "", b1, sizeof b1);
    bm_str_replace_all(b1, "{date}", full_date ? full_date : "", b2, sizeof b2);
    bm_str_replace_all(b2, "{month}", mon_str, b1, sizeof b1);
    bm_str_replace_all(b1, "{lastMonth}", lmon_str, b2, sizeof b2);
    bm_str_replace_all(b2, "{day}", day_str, out, cap);
}

void bm_template_process_today(
    const char *tmpl,
    const bm_guard *user,
    const char *code,
    char *out,
    size_t cap
) {
    int y, m, d;
    bm_date_today(&y, &m, &d);
    char full_date[16];
    snprintf(full_date, sizeof full_date, "%04d-%02d-%02d", y, m, d);
    int last_month = (m == 1) ? 12 : (m - 1);
    bm_template_process(tmpl, user, code, full_date, m, last_month, d, out, cap);
}
