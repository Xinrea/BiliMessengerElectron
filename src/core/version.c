#include "core/version.h"

#include <stddef.h>
#include <string.h>

typedef struct {
    const char *text;
    size_t len;
    bool numeric;
} version_part;

typedef struct {
    version_part core[3];
    const char *prerelease;
} version;

static bool is_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool read_identifier(const char **cursor, version_part *part) {
    const char *p = *cursor;
    part->text = p;
    part->numeric = true;
    while (is_digit(*p) || (*p >= 'A' && *p <= 'Z') ||
           (*p >= 'a' && *p <= 'z') || *p == '-') {
        if (!is_digit(*p)) part->numeric = false;
        p++;
    }
    part->len = (size_t)(p - part->text);
    *cursor = p;
    return part->len > 0;
}

static bool read_identifiers(const char **cursor, bool prerelease) {
    version_part part;
    do {
        if (!read_identifier(cursor, &part)) return false;
        if (prerelease && part.numeric && part.len > 1 && part.text[0] == '0') return false;
        if (**cursor != '.') return true;
        (*cursor)++;
    } while (true);
}

static bool parse_version(const char *text, version *out) {
    if (!text) return false;
    const char *p = text;
    if (*p == 'v') p++;
    for (int i = 0; i < 3; i++) {
        version_part *part = &out->core[i];
        part->text = p;
        part->numeric = true;
        while (is_digit(*p)) p++;
        part->len = (size_t)(p - part->text);
        if (!part->len || (part->len > 1 && part->text[0] == '0')) return false;
        if (i < 2) {
            if (*p != '.') return false;
            p++;
        }
    }
    out->prerelease = NULL;
    if (*p == '-') {
        out->prerelease = ++p;
        if (!read_identifiers(&p, true)) return false;
    }
    if (*p == '+') {
        p++;
        if (!read_identifiers(&p, false)) return false;
    }
    return *p == '\0';
}

static int compare_parts(version_part a, version_part b) {
    if (a.numeric != b.numeric) return a.numeric ? -1 : 1;
    /* Compare digit strings by length first, avoiding integer overflow. */
    if (a.numeric && a.len != b.len) return a.len < b.len ? -1 : 1;
    size_t len = a.len < b.len ? a.len : b.len;
    int cmp = memcmp(a.text, b.text, len);
    if (cmp) return cmp < 0 ? -1 : 1;
    return (a.len > b.len) - (a.len < b.len);
}

bool bm_version_compare(const char *a, const char *b, int *order) {
    version left, right;
    if (!order || !parse_version(a, &left) || !parse_version(b, &right)) return false;
    for (int i = 0; i < 3; i++) {
        int cmp = compare_parts(left.core[i], right.core[i]);
        if (cmp) {
            *order = cmp;
            return true;
        }
    }

    const char *p = left.prerelease, *q = right.prerelease;
    if (!p || !q) {
        /* A stable version has higher precedence than its prereleases. */
        *order = (p == NULL) - (q == NULL);
        return true;
    }
    for (;;) {
        version_part lp, rp;
        read_identifier(&p, &lp);
        read_identifier(&q, &rp);
        int cmp = compare_parts(lp, rp);
        if (cmp || *p != '.' || *q != '.') {
            *order = cmp ? cmp : (*p == '.') - (*q == '.');
            return true;
        }
        p++;
        q++;
    }
}
