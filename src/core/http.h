#ifndef BM_HTTP_H
#define BM_HTTP_H

#include <stdbool.h>
#include <stddef.h>

#define BM_MAX_SET_COOKIES 16

typedef struct bm_http_req {
    const char *url;
    const char *method;        /* NULL or "GET" or "POST" */
    const char *body;          /* optional POST body */
    size_t body_len;           /* 0 = strlen(body) if body != NULL */
    const char *cookie;        /* Cookie header value */
    const char *user_agent;    /* User-Agent header */
    const char *referer;       /* Referer header */
    const char *origin;        /* Origin header */
    const char *content_type;  /* Content-Type header */
    const char *accept;        /* Accept header */
    const char *const *extra_headers;
    int extra_header_count;
    int timeout_ms;            /* default 30000 */
    bool no_follow_redirect;   /* if true, do not automatically follow 3xx redirects */
} bm_http_req;

typedef struct bm_http_resp {
    bool ok;
    bool timed_out;
    long status;
    char *data;
    size_t size;
    char location[1024];
    char set_cookies[BM_MAX_SET_COOKIES][512];
    int set_cookie_count;
    char error[256];
} bm_http_resp;

typedef bool (*bm_http_mock_fn)(const bm_http_req *req, bm_http_resp *resp, void *user);

void bm_http_init(void);
void bm_http_cleanup(void);
void bm_http_set_mock(bm_http_mock_fn fn, void *user);

bool bm_http_request(const bm_http_req *req, bm_http_resp *resp);
void bm_http_resp_free(bm_http_resp *resp);

#endif /* BM_HTTP_H */
