#ifndef MYALLOC_H
#define MYALLOC_H

#include <stddef.h>

/* Placement policy used when searching the free list. */
typedef enum { FIT_FIRST, FIT_BEST } fit_policy_t;

/* Snapshot of the allocator's state (used by the tests and by heap_dump). */
typedef struct {
    size_t heap_bytes;
    size_t used_bytes;
    size_t free_bytes;
    size_t blocks;
    size_t free_blocks;
    size_t largest_free;
} heap_stats_t;

/* Required API */
void *my_malloc(size_t size);
void my_free(void *ptr);
void *my_calloc(size_t nmemb, size_t size);
void *my_realloc(void *ptr, size_t size);

/* Diagnostics and configuration */
void heap_stats(heap_stats_t *out);
void heap_dump(void);
void set_policy(fit_policy_t p);
int heap_check(void);

#endif
