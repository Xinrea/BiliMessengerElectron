#include "core/md5.h"

#include <string.h>

typedef struct {
    uint32_t state[4];
    uint64_t count;
    uint8_t buffer[64];
} md5_ctx;

#define ROL32(x, n) (((x) << (n)) | ((x) >> (32 - (n))))

static uint32_t load_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void store_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

static void md5_transform(uint32_t state[4], const uint8_t block[64]) {
    static const uint32_t k[64] = {
        0xd76aa478u, 0xe8c7b756u, 0x242070dbu, 0xc1bdceeeu,
        0xf57c0fafu, 0x4787c62au, 0xa8304613u, 0xfd469501u,
        0x698098d8u, 0x8b44f7afu, 0xffff5bb1u, 0x895cd7beu,
        0x6b901122u, 0xfd987193u, 0xa679438eu, 0x49b40821u,
        0xf61e2562u, 0xc040b340u, 0x265e5a51u, 0xe9b6c7aau,
        0xd62f105du, 0x02441453u, 0xd8a1e681u, 0xe7d3fbc8u,
        0x21e1cde6u, 0xc33707d6u, 0xf4d50d87u, 0x455a14edu,
        0xa9e3e905u, 0xfcefa3f8u, 0x676f02d9u, 0x8d2a4c8au,
        0xfffa3942u, 0x8771f681u, 0x6d9d6122u, 0xfde5380cu,
        0xa4beea44u, 0x4bdecfa9u, 0xf6bb4b60u, 0xbebfbc70u,
        0x289b7ec6u, 0xeaa127fau, 0xd4ef3085u, 0x04881d05u,
        0xd9d4d039u, 0xe6db99e5u, 0x1fa27cf8u, 0xc4ac5665u,
        0xf4292244u, 0x432aff97u, 0xab9423a7u, 0xfc93a039u,
        0x655b59c3u, 0x8f0ccc92u, 0xffeff47du, 0x85845dd1u,
        0x6fa87e4fu, 0xfe2ce6e0u, 0xa3014314u, 0x4e0811a1u,
        0xf7537e82u, 0xbd3af235u, 0x2ad7d2bbu, 0xeb86d391u
    };
    static const uint8_t s[64] = {
        7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
        5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20, 5,  9, 14, 20,
        4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
        6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
    };

    uint32_t m[16];
    for (int i = 0; i < 16; i++) {
        m[i] = load_le32(block + i * 4);
    }

    uint32_t a = state[0];
    uint32_t b = state[1];
    uint32_t c = state[2];
    uint32_t d = state[3];

    for (int i = 0; i < 64; i++) {
        uint32_t f, g;
        if (i < 16) {
            f = (b & c) | ((~b) & d);
            g = (uint32_t)i;
        } else if (i < 32) {
            f = (d & b) | ((~d) & c);
            g = (uint32_t)((5 * i + 1) & 15);
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (uint32_t)((3 * i + 5) & 15);
        } else {
            f = c ^ (b | (~d));
            g = (uint32_t)((7 * i) & 15);
        }
        uint32_t temp = d;
        d = c;
        c = b;
        b = b + ROL32(a + f + k[i] + m[g], s[i]);
        a = temp;
    }

    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

void bm_md5(const void *data, size_t len, uint8_t digest[16]) {
    md5_ctx ctx;
    ctx.state[0] = 0x67452301u;
    ctx.state[1] = 0xefcdab89u;
    ctx.state[2] = 0x98badcfeu;
    ctx.state[3] = 0x10325476u;
    ctx.count = (uint64_t)len * 8;

    const uint8_t *bytes = (const uint8_t *)data;
    size_t offset = 0;
    while (len - offset >= 64) {
        md5_transform(ctx.state, bytes + offset);
        offset += 64;
    }

    size_t rem = len - offset;
    memset(ctx.buffer, 0, sizeof ctx.buffer);
    if (rem > 0) {
        memcpy(ctx.buffer, bytes + offset, rem);
    }
    ctx.buffer[rem] = 0x80;

    if (rem >= 56) {
        md5_transform(ctx.state, ctx.buffer);
        memset(ctx.buffer, 0, sizeof ctx.buffer);
    }

    store_le32(ctx.buffer + 56, (uint32_t)(ctx.count & 0xFFFFFFFFu));
    store_le32(ctx.buffer + 60, (uint32_t)(ctx.count >> 32));
    md5_transform(ctx.state, ctx.buffer);

    for (int i = 0; i < 4; i++) {
        store_le32(digest + i * 4, ctx.state[i]);
    }
}

void bm_md5_hex(const void *data, size_t len, char out_hex[33]) {
    static const char hex[] = "0123456789abcdef";
    uint8_t digest[16];
    bm_md5(data, len, digest);
    for (int i = 0; i < 16; i++) {
        out_hex[i * 2] = hex[(digest[i] >> 4) & 0x0F];
        out_hex[i * 2 + 1] = hex[digest[i] & 0x0F];
    }
    out_hex[32] = '\0';
}
