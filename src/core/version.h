#ifndef BM_VERSION_H
#define BM_VERSION_H

#include <stdbool.h>

/* Compare SemVer 2.0 versions, allowing an optional leading 'v'.
 * Build metadata is ignored. On success, order is -1, 0, or 1 for a vs b.
 * Invalid versions (or a NULL argument) return false without changing order. */
bool bm_version_compare(const char *a, const char *b, int *order);

#endif /* BM_VERSION_H */
