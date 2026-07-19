#ifndef _FROG_REFCOUNT_H
#define _FROG_REFCOUNT_H

#include <frog/types.h>

typedef struct {
        volatile uint_32 value;
} refcount_t;

void refcount_init(refcount_t *ref, uint_32 value);

/* Diagnostic only unless the caller provides the object's lifetime lock. */
uint_32 refcount_read(const refcount_t *ref);

/* Increment a live reference; zero and saturated counts are rejected. */
bool refcount_get_live(refcount_t *ref);

/* Drop a reference and return true only for the 1 -> 0 transition. */
bool refcount_put(refcount_t *ref);

#endif
