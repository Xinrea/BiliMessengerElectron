#ifndef BM_MD5_H
#define BM_MD5_H

#include <stddef.h>
#include <stdint.h>

void bm_md5(const void *data, size_t len, uint8_t digest[16]);
void bm_md5_hex(const void *data, size_t len, char out_hex[33]);

#endif /* BM_MD5_H */
