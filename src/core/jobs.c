#include "core/jobs.h"
#include "core/diagnostics.h"

#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <process.h>
#include <windows.h>
#else
#include <pthread.h>
#endif

typedef struct bm_posted_event {
    bm_main_cb cb;
    void *data;
    struct bm_posted_event *next;
} bm_posted_event;

#if defined(_WIN32)
static CRITICAL_SECTION g_lock;
#else
static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
#endif
static bool g_inited = false;
static int g_active_jobs = 0;
static bm_posted_event *g_head = NULL;
static bm_posted_event *g_tail = NULL;

static void lock_queue(void) {
#if defined(_WIN32)
    EnterCriticalSection(&g_lock);
#else
    pthread_mutex_lock(&g_lock);
#endif
}

static void unlock_queue(void) {
#if defined(_WIN32)
    LeaveCriticalSection(&g_lock);
#else
    pthread_mutex_unlock(&g_lock);
#endif
}

void bm_jobs_init(void) {
    if (g_inited) return;
#if defined(_WIN32)
    InitializeCriticalSection(&g_lock);
#endif
    g_inited = true;
}

void bm_jobs_shutdown(void) {
    if (!g_inited) return;
    lock_queue();
    bm_posted_event *cur = g_head;
    g_head = g_tail = NULL;
    unlock_queue();
    while (cur) {
        bm_posted_event *next = cur->next;
        free(cur);
        cur = next;
    }
}

typedef struct {
    bm_job_fn fn;
    void *arg;
} job_runner_arg;

#if defined(_WIN32)
static unsigned __stdcall job_thread_entry(void *raw) {
    job_runner_arg *ja = (job_runner_arg *)raw;
    bm_diagnostics_event(BM_EVENT_JOB_START, BM_LOG_INFO, 0, 0);
    ja->fn(ja->arg);
    bm_diagnostics_event(BM_EVENT_JOB_FINISH, BM_LOG_INFO, 0, 0);
    free(ja);
    lock_queue();
    g_active_jobs--;
    unlock_queue();
    return 0;
}
#else
static void *job_thread_entry(void *raw) {
    job_runner_arg *ja = (job_runner_arg *)raw;
    bm_diagnostics_event(BM_EVENT_JOB_START, BM_LOG_INFO, 0, 0);
    ja->fn(ja->arg);
    bm_diagnostics_event(BM_EVENT_JOB_FINISH, BM_LOG_INFO, 0, 0);
    free(ja);
    lock_queue();
    g_active_jobs--;
    unlock_queue();
    return NULL;
}
#endif

bool bm_job_spawn(bm_job_fn fn, void *arg) {
    if (!fn) {
        bm_diagnostics_event(BM_EVENT_JOB_FAILED, BM_LOG_ERROR, 1, 0);
        return false;
    }
    bm_jobs_init();
    job_runner_arg *ja = (job_runner_arg *)malloc(sizeof *ja);
    if (!ja) {
        bm_diagnostics_event(BM_EVENT_JOB_FAILED, BM_LOG_ERROR, 2, 0);
        return false;
    }
    ja->fn = fn;
    ja->arg = arg;

    lock_queue();
    g_active_jobs++;
    unlock_queue();

#if defined(_WIN32)
    uintptr_t h = _beginthreadex(NULL, 0, job_thread_entry, ja, 0, NULL);
    if (!h) {
        bm_diagnostics_event(BM_EVENT_JOB_FAILED, BM_LOG_ERROR, 3, 0);
        lock_queue();
        g_active_jobs--;
        unlock_queue();
        free(ja);
        return false;
    }
    CloseHandle((HANDLE)h);
    return true;
#else
    pthread_t tid;
    if (pthread_create(&tid, NULL, job_thread_entry, ja) != 0) {
        bm_diagnostics_event(BM_EVENT_JOB_FAILED, BM_LOG_ERROR, 3, 0);
        lock_queue();
        g_active_jobs--;
        unlock_queue();
        free(ja);
        return false;
    }
    pthread_detach(tid);
    return true;
#endif
}

void bm_job_post_main(bm_main_cb cb, void *data) {
    if (!cb) return;
    bm_jobs_init();
    bm_posted_event *ev = (bm_posted_event *)malloc(sizeof *ev);
    if (!ev) {
        bm_diagnostics_event(BM_EVENT_CALLBACK_FAILED, BM_LOG_ERROR, 2, 0);
        return;
    }
    ev->cb = cb;
    ev->data = data;
    ev->next = NULL;

    lock_queue();
    if (g_tail) {
        g_tail->next = ev;
        g_tail = ev;
    } else {
        g_head = g_tail = ev;
    }
    unlock_queue();
}

int bm_jobs_poll_main(void *user) {
    if (!g_inited) return 0;
    lock_queue();
    bm_posted_event *list = g_head;
    g_head = g_tail = NULL;
    unlock_queue();

    int count = 0;
    while (list) {
        bm_posted_event *next = list->next;
        list->cb(list->data, user);
        free(list);
        list = next;
        count++;
    }
    return count;
}

int bm_jobs_active_count(void) {
    if (!g_inited) return 0;
    lock_queue();
    int c = g_active_jobs;
    if (g_head) c++;
    unlock_queue();
    return c;
}
