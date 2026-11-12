#include "core/store.h"
#include "core/diagnostics.h"
#include "core/util.h"
#include "cJSON.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void bm_store_init(bm_store *s) {
    memset(s, 0, sizeof *s);
    s->send_interval = 1000;
    s->dark_mode = true;
    bm_default_config_path(s->path, sizeof s->path);
    bm_guard_list_init(&s->original_guards);
    bm_guard_list_init(&s->guards);
}

void bm_store_free(bm_store *s) {
    if (!s) return;
    bm_guard_list_free(&s->original_guards);
    bm_guard_list_free(&s->guards);
    free(s->templates);
    s->templates = NULL;
    s->template_count = s->template_cap = 0;
    free(s->codes);
    s->codes = NULL;
    s->code_count = s->code_cap = 0;
}

static void json_get_str(const cJSON *obj, const char *key, char *out, size_t cap) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(item) && item->valuestring) {
        bm_strlcpy(out, item->valuestring, cap);
    } else if (cJSON_IsNumber(item)) {
        snprintf(out, cap, "%.0f", item->valuedouble);
    }
}

static uint64_t json_get_u64(const cJSON *obj, const char *key) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item)) {
        return (uint64_t)item->valuedouble;
    }
    if (cJSON_IsString(item) && item->valuestring) {
        return (uint64_t)strtoull(item->valuestring, NULL, 10);
    }
    return 0;
}

static int json_get_int(const cJSON *obj, const char *key, int def_val) {
    const cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item)) return item->valueint;
    if (cJSON_IsString(item) && item->valuestring) return atoi(item->valuestring);
    return def_val;
}

static void parse_guard_array(const cJSON *arr, bm_guard_list *list) {
    bm_guard_list_clear(list);
    if (!cJSON_IsArray(arr)) return;
    const cJSON *el = NULL;
    cJSON_ArrayForEach(el, arr) {
        if (!cJSON_IsObject(el)) continue;
        bm_guard g;
        memset(&g, 0, sizeof g);
        json_get_str(el, "face", g.face, sizeof g.face);
        json_get_str(el, "username", g.username, sizeof g.username);
        json_get_str(el, "time", g.time, sizeof g.time);
        g.is_alive = json_get_int(el, "is_alive", 0);
        g.rank = json_get_int(el, "rank", 1);
        g.ruid = json_get_u64(el, "ruid");
        g.uid = json_get_u64(el, "uid");

        const cJSON *lvl = cJSON_GetObjectItemCaseSensitive(el, "guard_level");
        if (cJSON_IsNumber(lvl)) {
            int lv = lvl->valueint;
            if (lv >= 1 && lv <= 3) {
                g.guard_level = lv;
            } else {
                g.guard_level = 0;
                bm_strlcpy(g.custom_title, "自定义", sizeof g.custom_title);
            }
        } else if (cJSON_IsString(lvl) && lvl->valuestring) {
            bm_guard_set_level_from_tag(&g, lvl->valuestring);
        } else {
            g.guard_level = 3;
        }
        bm_guard_list_push(list, &g);
    }
}

static cJSON *serialize_guard_array(const bm_guard_list *list) {
    cJSON *arr = cJSON_CreateArray();
    if (!arr || !list) return arr;
    for (int i = 0; i < list->count; i++) {
        const bm_guard *g = &list->items[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "face", g->face[0] ? g->face : "./static/noface.jpg");
        cJSON_AddStringToObject(obj, "username", g->username);
        if (g->custom_title[0]) {
            cJSON_AddStringToObject(obj, "guard_level", g->custom_title);
        } else {
            cJSON_AddNumberToObject(obj, "guard_level", g->guard_level);
        }
        cJSON_AddNumberToObject(obj, "is_alive", g->is_alive);
        cJSON_AddNumberToObject(obj, "rank", g->rank);
        cJSON_AddNumberToObject(obj, "ruid", (double)(g->ruid ? g->ruid : 61639371ULL));
        cJSON_AddNumberToObject(obj, "uid", (double)g->uid);
        if (g->time[0]) {
            cJSON_AddStringToObject(obj, "time", g->time);
        }
        cJSON *medal = cJSON_CreateObject();
        cJSON_AddNumberToObject(medal, "medal_level", 27);
        cJSON_AddStringToObject(medal, "medal_name", "轴芯");
        cJSON_AddItemToObject(obj, "medal_info", medal);
        cJSON_AddItemToArray(arr, obj);
    }
    return arr;
}

static bool ensure_template_cap(bm_store *s, int needed) {
    if (needed <= s->template_cap) return true;
    int next_cap = s->template_cap ? s->template_cap * 2 : 8;
    while (next_cap < needed) next_cap *= 2;
    bm_template *next = (bm_template *)realloc(s->templates, (size_t)next_cap * sizeof(bm_template));
    if (!next) return false;
    s->templates = next;
    s->template_cap = next_cap;
    return true;
}

static bool ensure_code_cap(bm_store *s, int needed) {
    if (needed <= s->code_cap) return true;
    int next_cap = s->code_cap ? s->code_cap * 2 : 32;
    while (next_cap < needed) next_cap *= 2;
    bm_code_item *next = (bm_code_item *)realloc(s->codes, (size_t)next_cap * sizeof(bm_code_item));
    if (!next) return false;
    s->codes = next;
    s->code_cap = next_cap;
    return true;
}

static bool store_load(bm_store *s, const char *path) {
    if (!s) return false;
    if (path && path[0]) {
        bm_strlcpy(s->path, path, sizeof s->path);
    }
    if (!s->path[0]) return false;

    char *buf = NULL;
    size_t len = 0;
    if (!bm_read_file(s->path, &buf, &len)) {
        return false;
    }

    cJSON *root = cJSON_Parse(buf);
    free(buf);
    if (!root || !cJSON_IsObject(root)) {
        if (root) cJSON_Delete(root);
        return false;
    }

    json_get_str(root, "roomID", s->room_id, sizeof s->room_id);

    const cJSON *setting = cJSON_GetObjectItemCaseSensitive(root, "setting");
    if (cJSON_IsObject(setting)) {
        s->send_interval = json_get_int(setting, "sendInterval", 1000);
        if (s->send_interval <= 0) s->send_interval = 1000;
        const cJSON *dm = cJSON_GetObjectItemCaseSensitive(setting, "darkMode");
        if (cJSON_IsBool(dm)) s->dark_mode = cJSON_IsTrue(dm);
    }

    memset(&s->login, 0, sizeof s->login);
    const cJSON *login = cJSON_GetObjectItemCaseSensitive(root, "loginResponse");
    if (cJSON_IsObject(login)) {
        json_get_str(login, "SESSDATA", s->login.SESSDATA, sizeof s->login.SESSDATA);
        bm_normalize_sessdata(s->login.SESSDATA, s->login.SESSDATA, sizeof s->login.SESSDATA);
        json_get_str(login, "DedeUserID", s->login.DedeUserID, sizeof s->login.DedeUserID);
        json_get_str(login, "DedeUserID__ckMd5", s->login.DedeUserID__ckMd5, sizeof s->login.DedeUserID__ckMd5);
        json_get_str(login, "bili_jct", s->login.bili_jct, sizeof s->login.bili_jct);
        json_get_str(login, "sid", s->login.sid, sizeof s->login.sid);
        json_get_str(login, "buvid3", s->login.buvid3, sizeof s->login.buvid3);
        json_get_str(login, "buvid4", s->login.buvid4, sizeof s->login.buvid4);
        json_get_str(login, "_uuid", s->login._uuid, sizeof s->login._uuid);
        json_get_str(login, "b_nut", s->login.b_nut, sizeof s->login.b_nut);
        json_get_str(login, "b_lsid", s->login.b_lsid, sizeof s->login.b_lsid);
        json_get_str(login, "bili_ticket", s->login.bili_ticket, sizeof s->login.bili_ticket);
        json_get_str(login, "dev_id", s->login.dev_id, sizeof s->login.dev_id);
        json_get_str(login, "cookies", s->login.cookies, sizeof s->login.cookies);
        s->login.has_login = bm_login_is_complete(&s->login);
    }

    parse_guard_array(cJSON_GetObjectItemCaseSensitive(root, "originalGuards"), &s->original_guards);
    parse_guard_array(cJSON_GetObjectItemCaseSensitive(root, "guards"), &s->guards);
    if (s->original_guards.count == 0 && s->guards.count > 0) {
        bm_guard_list_copy(&s->original_guards, &s->guards);
    }

    s->template_count = 0;
    const cJSON *tlist = cJSON_GetObjectItemCaseSensitive(root, "templateList");
    if (cJSON_IsArray(tlist)) {
        const cJSON *tname = NULL;
        cJSON_ArrayForEach(tname, tlist) {
            if (!cJSON_IsString(tname) || !tname->valuestring || !tname->valuestring[0]) continue;
            char key[160];
            snprintf(key, sizeof key, "template:%s", tname->valuestring);
            char val[4096] = { 0 };
            json_get_str(root, key, val, sizeof val);
            bm_store_set_template(s, tname->valuestring, val);
        }
    }

    s->code_count = 0;
    const cJSON *codes = cJSON_GetObjectItemCaseSensitive(root, "codes");
    if (cJSON_IsArray(codes)) {
        const cJSON *c = NULL;
        cJSON_ArrayForEach(c, codes) {
            if (cJSON_IsString(c) && c->valuestring && c->valuestring[0]) {
                if (ensure_code_cap(s, s->code_count + 1)) {
                    memset(&s->codes[s->code_count], 0, sizeof(bm_code_item));
                    bm_strlcpy(s->codes[s->code_count].code, c->valuestring, sizeof s->codes[0].code);
                    s->code_count++;
                }
            }
        }
    }

    cJSON_Delete(root);
    return true;
}

bool bm_store_load(bm_store *s, const char *path) {
    bool ok = store_load(s, path);
    bm_diagnostics_event(BM_EVENT_CONFIG_LOAD, ok ? BM_LOG_INFO : BM_LOG_WARNING, ok ? 0 : 1, 0);
    return ok;
}

static bool store_save(const bm_store *s) {
    if (!s || !s->path[0]) return false;

    cJSON *root = cJSON_CreateObject();
    if (!root) return false;

    cJSON_AddStringToObject(root, "roomID", s->room_id);

    cJSON *setting = cJSON_CreateObject();
    cJSON_AddNumberToObject(setting, "sendInterval", s->send_interval > 0 ? s->send_interval : 1000);
    cJSON_AddBoolToObject(setting, "darkMode", s->dark_mode);
    cJSON_AddItemToObject(root, "setting", setting);

    if (s->login.has_login && bm_login_is_complete(&s->login)) {
        cJSON *login = cJSON_CreateObject();
        cJSON_AddStringToObject(login, "SESSDATA", s->login.SESSDATA);
        cJSON_AddStringToObject(login, "DedeUserID", s->login.DedeUserID);
        if (s->login.DedeUserID__ckMd5[0]) cJSON_AddStringToObject(login, "DedeUserID__ckMd5", s->login.DedeUserID__ckMd5);
        cJSON_AddStringToObject(login, "bili_jct", s->login.bili_jct);
        if (s->login.sid[0]) cJSON_AddStringToObject(login, "sid", s->login.sid);
        if (s->login.buvid3[0]) cJSON_AddStringToObject(login, "buvid3", s->login.buvid3);
        if (s->login.buvid4[0]) cJSON_AddStringToObject(login, "buvid4", s->login.buvid4);
        if (s->login._uuid[0]) cJSON_AddStringToObject(login, "_uuid", s->login._uuid);
        if (s->login.b_nut[0]) cJSON_AddStringToObject(login, "b_nut", s->login.b_nut);
        if (s->login.b_lsid[0]) cJSON_AddStringToObject(login, "b_lsid", s->login.b_lsid);
        if (s->login.bili_ticket[0]) cJSON_AddStringToObject(login, "bili_ticket", s->login.bili_ticket);
        if (s->login.dev_id[0]) cJSON_AddStringToObject(login, "dev_id", s->login.dev_id);
        if (s->login.cookies[0]) cJSON_AddStringToObject(login, "cookies", s->login.cookies);
        cJSON_AddItemToObject(root, "loginResponse", login);
    } else {
        cJSON_AddNullToObject(root, "loginResponse");
    }

    cJSON_AddItemToObject(root, "originalGuards", serialize_guard_array(&s->original_guards));
    cJSON_AddItemToObject(root, "guards", serialize_guard_array(&s->guards));

    cJSON *tlist = cJSON_CreateArray();
    for (int i = 0; i < s->template_count; i++) {
        cJSON_AddItemToArray(tlist, cJSON_CreateString(s->templates[i].name));
        char key[160];
        snprintf(key, sizeof key, "template:%s", s->templates[i].name);
        cJSON_AddStringToObject(root, key, s->templates[i].value);
    }
    cJSON_AddItemToObject(root, "templateList", tlist);

    cJSON *codes = cJSON_CreateArray();
    for (int i = 0; i < s->code_count; i++) {
        cJSON_AddItemToArray(codes, cJSON_CreateString(s->codes[i].code));
    }
    cJSON_AddItemToObject(root, "codes", codes);

    char *text = cJSON_Print(root);
    cJSON_Delete(root);
    if (!text) return false;
    bool ok = bm_write_file(s->path, text, strlen(text));
    free(text);
    return ok;
}

bool bm_store_save(const bm_store *s) {
    bool ok = store_save(s);
    bm_diagnostics_event(BM_EVENT_CONFIG_SAVE, ok ? BM_LOG_INFO : BM_LOG_ERROR, ok ? 0 : 1, 0);
    return ok;
}

bool bm_login_is_complete(const bm_login_info *login) {
    if (!login) return false;
    return login->SESSDATA[0] != '\0' &&
           login->DedeUserID[0] != '\0' &&
           login->bili_jct[0] != '\0';
}

void bm_store_clear_login(bm_store *s) {
    if (!s) return;
    memset(&s->login, 0, sizeof s->login);
}

int bm_store_find_template(const bm_store *s, const char *name) {
    if (!s || !name || !name[0]) return -1;
    for (int i = 0; i < s->template_count; i++) {
        if (strcmp(s->templates[i].name, name) == 0) return i;
    }
    return -1;
}

const char *bm_store_get_template(const bm_store *s, const char *name) {
    int idx = bm_store_find_template(s, name);
    if (idx < 0) return "";
    return s->templates[idx].value;
}

void bm_store_set_template(bm_store *s, const char *name, const char *value) {
    if (!s) return;
    if (!name || !name[0]) name = "未命名";
    int idx = bm_store_find_template(s, name);
    if (idx >= 0) {
        bm_strlcpy(s->templates[idx].value, value ? value : "", sizeof s->templates[idx].value);
        return;
    }
    if (!ensure_template_cap(s, s->template_count + 1)) return;
    bm_template *t = &s->templates[s->template_count++];
    bm_strlcpy(t->name, name, sizeof t->name);
    bm_strlcpy(t->value, value ? value : "", sizeof t->value);
}

bool bm_store_delete_template(bm_store *s, const char *name) {
    int idx = bm_store_find_template(s, name);
    if (idx < 0) return false;
    if (idx + 1 < s->template_count) {
        memmove(&s->templates[idx], &s->templates[idx + 1], (size_t)(s->template_count - idx - 1) * sizeof(bm_template));
    }
    s->template_count--;
    return true;
}

int bm_store_import_codes(bm_store *s, const char *multiline_text) {
    if (!s || !multiline_text) return 0;
    int added = 0;
    const char *p = multiline_text;
    while (*p) {
        const char *nl = strchr(p, '\n');
        size_t len = nl ? (size_t)(nl - p) : strlen(p);
        char line[256];
        if (len >= sizeof line) len = sizeof line - 1;
        memcpy(line, p, len);
        line[len] = '\0';
        if (len > 0 && line[len - 1] == '\r') line[--len] = '\0';
        p = nl ? nl + 1 : p + len;

        if (!line[0]) continue;
        bool exists = false;
        for (int i = 0; i < s->code_count; i++) {
            if (strcmp(s->codes[i].code, line) == 0) {
                exists = true;
                break;
            }
        }
        if (!exists && ensure_code_cap(s, s->code_count + 1)) {
            memset(&s->codes[s->code_count], 0, sizeof(bm_code_item));
            bm_strlcpy(s->codes[s->code_count].code, line, sizeof s->codes[0].code);
            s->code_count++;
            added++;
        }
    }
    return added;
}

int bm_store_delete_selected_codes(bm_store *s) {
    if (!s) return 0;
    int w = 0;
    for (int r = 0; r < s->code_count; r++) {
        if (!s->codes[r].selected) {
            if (w != r) s->codes[w] = s->codes[r];
            w++;
        }
    }
    int removed = s->code_count - w;
    s->code_count = w;
    return removed;
}

bool bm_store_pop_first_code(bm_store *s) {
    if (!s || s->code_count <= 0) return false;
    if (s->code_count > 1) {
        memmove(&s->codes[0], &s->codes[1], (size_t)(s->code_count - 1) * sizeof(bm_code_item));
    }
    s->code_count--;
    return true;
}

bool bm_store_consume_code(bm_store *s, const char *code) {
    if (!s || !code || !code[0]) return false;
    for (int i = 0; i < s->code_count; i++) {
        if (strcmp(s->codes[i].code, code) != 0) continue;
        if (i + 1 < s->code_count) {
            memmove(&s->codes[i], &s->codes[i + 1], (size_t)(s->code_count - i - 1) * sizeof(bm_code_item));
        }
        s->code_count--;
        return true;
    }
    return false;
}

void bm_store_clear_codes(bm_store *s) {
    if (!s) return;
    s->code_count = 0;
}
