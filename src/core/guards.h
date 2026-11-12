#ifndef BM_GUARDS_H
#define BM_GUARDS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct bm_guard {
    uint64_t uid;
    uint64_t ruid;
    char username[128];
    char face[512];
    int guard_level;       /* 1: 总督, 2: 提督, 3: 舰长, 0: custom */
    char custom_title[64]; /* non-empty when custom title */
    int is_alive;
    int rank;
    char time[64];
    bool selected;
} bm_guard;

typedef struct bm_guard_list {
    bm_guard *items;
    int count;
    int cap;
} bm_guard_list;

typedef struct bm_guard_stats {
    int total;
    int type1;
    int type2;
    int type3;
} bm_guard_stats;

typedef struct bm_guard_filters {
    bool type1;
    bool type2;
    bool type3;
} bm_guard_filters;

void bm_guard_list_init(bm_guard_list *list);
void bm_guard_list_free(bm_guard_list *list);
void bm_guard_list_clear(bm_guard_list *list);
bool bm_guard_list_push(bm_guard_list *list, const bm_guard *g);
bool bm_guard_list_unshift(bm_guard_list *list, const bm_guard *g);
bool bm_guard_list_copy(bm_guard_list *dst, const bm_guard_list *src);
int bm_guard_list_find_uid(const bm_guard_list *list, uint64_t uid);
bool bm_guard_list_remove_at(bm_guard_list *list, int index);
int bm_guard_list_remove_uid(bm_guard_list *list, uint64_t uid);

/* Record one finished private-message attempt.
   A delivered recipient is removed from both the persisted roster and the live
   queue. A failure is removed from the live queue only, so this pass does not
   retry it, while a later rebuild of the queue from the roster still includes
   them. Removal is by uid: the live queue may have been reordered while the
   request was in flight, and index 0 is then a different person.
   Invalid uid 0 advances one queue entry without deleting roster records. */
void bm_guard_note_send_result(
    bm_guard_list *roster,
    bm_guard_list *queue,
    const bm_guard *target,
    bool delivered
);

const char *bm_guard_level_str(const bm_guard *g);
int bm_gift_name_to_level(const char *gift_name);
void bm_guard_set_level_from_tag(bm_guard *g, const char *tag);

void bm_guard_compute_stats(const bm_guard_list *list, bm_guard_stats *stats);
void bm_guard_apply_filters(const bm_guard_list *src, const bm_guard_filters *filters, bm_guard_list *dst);
/* Apply removals/filter exclusions without re-enqueuing attempted recipients. */
void bm_guard_prune_queue(const bm_guard_list *roster, const bm_guard_filters *filters, bm_guard_list *queue);
int bm_guard_deduplicate(bm_guard_list *list);

char *bm_guard_format_txt(const bm_guard_list *list, size_t *out_len);
char *bm_guard_format_csv(const bm_guard_list *list, size_t *out_len);
bool bm_guard_export_txt(const bm_guard_list *list, const char *path);
bool bm_guard_export_csv(const bm_guard_list *list, const char *path);
bool bm_guard_import_text(bm_guard_list *list, const char *content, bool is_txt);
bool bm_guard_import_file(bm_guard_list *list, const char *path);

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
);

void bm_template_process_today(
    const char *tmpl,
    const bm_guard *user,
    const char *code,
    char *out,
    size_t cap
);

#endif /* BM_GUARDS_H */
