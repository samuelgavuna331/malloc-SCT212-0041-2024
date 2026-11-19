
#define _DEFAULT_SOURCE

#include <stddef.h>
#include <stdint.h>

/* Every returned payload must be aligned to 16 bytes. */
#define ALIGNMENT 16

/* Used to identify valid block headers. */
#define BLOCK_MAGIC 0x4D59414C

typedef struct block {
    size_t size;
    struct block *next;
    int free;
    int magic;
} block_t;

/* Round the header size up to a multiple of 16. */
#define HDR ((sizeof(block_t) + ALIGNMENT - 1) & ~(size_t)(ALIGNMENT - 1))

/* Round a requested size up to a multiple of 16. */
static inline size_t align_up(size_t n)
{
    if (n > SIZE_MAX - (ALIGNMENT - 1))
        return 0;

    return (n + ALIGNMENT - 1) & ~(size_t)(ALIGNMENT - 1);
}

/* Convert a block header address into its payload address. */
static inline void *payload(block_t *block)
{
    return (char *)block + HDR;
}

/* Convert a user payload pointer back into its block header. */
static inline block_t *header(void *ptr)
{
    return (block_t *)((char *)ptr - HDR);
}

/* Ensure the header size is a multiple of 16. */
_Static_assert(HDR % ALIGNMENT == 0,
               "Block header size must be 16-byte aligned");
