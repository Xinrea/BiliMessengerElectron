#ifndef BM_STORE_H
#define BM_STORE_H

#include "core/guards.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct bm_login_info {
    bool has_login;
    char SESSDATA[512];
    char DedeUserID[64];
    char DedeUserID__ckMd5[64];
    char bili_jct[128];
    char sid[64];
    char buvid3[128];
    char buvid4[128];
    char _uuid[128];
    char b_nut[64];
    char b_lsid[128];
    char bili_ticket[512];
    char dev_id[128];
    char cookies[2048];
} bm_login_info;

typedef struct bm_template {
    char name[128];
    char value[4096];
} bm_template;

typedef struct bm_code_item {
    char code[256];
    bool selected;
} bm_code_item;

typedef struct bm_store {
    char path[1024];
    char room_id[64];
    int send_interval;
    bool dark_mode;

    bm_login_info login;
    bm_guard_list original_guards;
    bm_guard_list guards;

    bm_template *templates;
    int template_count;
    int template_cap;

    bm_code_item *codes;
    int code_count;
    int code_cap;
} bm_store;

void bm_store_init(bm_store *s);
void bm_store_free(bm_store *s);

bool bm_store_load(bm_store *s, const char *path);
bool bm_store_save(const bm_store *s);

bool bm_login_is_complete(const bm_login_info *login);
void bm_store_clear_login(bm_store *s);

int bm_store_find_template(const bm_store *s, const char *name);
const char *bm_store_get_template(const bm_store *s, const char *name);
void bm_store_set_template(bm_store *s, const char *name, const char *value);
bool bm_store_delete_template(bm_store *s, const char *name);

int bm_store_import_codes(bm_store *s, const char *multiline_text);
int bm_store_delete_selected_codes(bm_store *s);
bool bm_store_pop_first_code(bm_store *s);
void bm_store_clear_codes(bm_store *s);

#endif /* BM_STORE_H */
