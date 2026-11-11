#ifndef BM_FEEDBACK_H
#define BM_FEEDBACK_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "core/diagnostics.h"

/* Anonymous feedback. No account identifiers or credentials belong here.
   A nonempty submission_id freezes all report fields until explicitly edited. */
typedef struct bm_feedback {
    char message[20001]; /* 5000 Unicode code points, up to four UTF-8 bytes each */
    char email[255];
    char submission_id[37];
    char app_version[101];
    char os[201];
    char receipt[37];
    int64_t retry_at_ms;
    bool include_log;
    char log[BM_DIAGNOSTIC_LOG_CAP]; /* Persisted, immutable UTF-8 attachment bytes. */
} bm_feedback;

typedef struct bm_feedback_result {
    bool ok;
    bool retryable;
    long status;
    char code[64];
    char message[384];
} bm_feedback_result;

/* Validates and trims a new draft; never call on an already frozen report. */
bool bm_feedback_validate(bm_feedback *draft, char *error, size_t cap);
/* Capture for preview; freeze reuses a previewed snapshot instead of replacing it. */
void bm_feedback_capture_log(bm_feedback *draft);
void bm_feedback_freeze(bm_feedback *draft);
void bm_feedback_edit(bm_feedback *draft);
/* One explicit attempt: prepare, then upload if needed. Never automatically retries.
   Always re-prepare the original ID, so credentials need not be persisted to disk. */
void bm_feedback_send(bm_feedback *draft, bm_feedback_result *result,
                      void (*on_upload)(void *), void *user);

/* Dedicated local file, separate from account configuration. Missing file is OK.
   Writes atomically; no upload token or server response body is stored. */
bool bm_feedback_load(bm_feedback *draft, const char *path);
bool bm_feedback_save(const bm_feedback *draft, const char *path);

#endif
