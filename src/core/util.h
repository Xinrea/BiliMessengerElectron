#ifndef BM_UTIL_H
#define BM_UTIL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

size_t bm_strlcpy(char *dst, const char *src, size_t cap);
char *bm_strdup(const char *s);
char *bm_trim(char *s);

void bm_str_replace_all(const char *src, const char *needle, const char *repl, char *out, size_t cap);
void bm_url_encode(const char *src, char *out, size_t cap);
void bm_url_encode_wbi(const char *src, char *out, size_t cap);
void bm_url_decode(const char *src, char *out, size_t cap);
void bm_normalize_sessdata(const char *src, char *out, size_t cap);
void bm_uuid_v4(char *out, size_t cap, bool uppercase);

bool bm_is_leap_year(int year);
int bm_days_in_month(int year, int month);
bool bm_parse_ymd(const char *s, int *year, int *month, int *day);
void bm_date_today(int *year, int *month, int *day);
void bm_date_today_str(char *out, size_t cap);
void bm_unix_to_ymd(int64_t unix_sec, char *out, size_t cap);
void bm_unix_to_mmdd(int64_t unix_sec, char *out, size_t cap);
int64_t bm_ymd_to_days(int year, int month, int day);

int64_t bm_now_ms(void);
void bm_sleep_ms(int ms);
void bm_set_time_hooks(int64_t (*now_ms_fn)(void *user), void (*sleep_ms_fn)(int ms, void *user), void *user);

bool bm_ensure_dir(const char *path);
bool bm_default_config_path(char *out, size_t cap);
bool bm_read_file(const char *path, char **out_buf, size_t *out_len);
bool bm_write_file(const char *path, const void *data, size_t len);

#endif /* BM_UTIL_H */
