#ifndef BM_JOBS_H
#define BM_JOBS_H

#include <stdbool.h>
#include <stddef.h>

typedef void (*bm_job_fn)(void *arg);
typedef void (*bm_main_cb)(void *data, void *user);

void bm_jobs_init(void);
void bm_jobs_shutdown(void);

bool bm_job_spawn(bm_job_fn fn, void *arg);
void bm_job_post_main(bm_main_cb cb, void *data);
int bm_jobs_poll_main(void *user);
int bm_jobs_active_count(void);

#endif /* BM_JOBS_H */
