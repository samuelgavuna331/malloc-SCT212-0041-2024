
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
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
/* Free-list boundaries. */
static block_t *free_head = NULL;
static block_t *free_tail = NULL;

/* Total bytes requested from the operating system. */
static size_t heap_bytes = 0;

/* Obtain more memory from the operating system. */
static block_t *grow(size_t need)
{
    const size_t CHUNK_SIZE = 64 * 1024;

    /* Make sure the new block can hold its header and payload. */
    if (need > SIZE_MAX - HDR)
        return NULL;

    size_t total = need + HDR;

    /* Obtain at least 64 KB unless the request is larger. */
    if (total < CHUNK_SIZE)
        total = CHUNK_SIZE;

    /* Find the current program break. */
    void *current_break = sbrk(0);

    if (current_break == (void *)-1)
        return NULL;

    /* Calculate padding needed to align the block header. */
    uintptr_t address = (uintptr_t)current_break;
    size_t padding = (ALIGNMENT - (address % ALIGNMENT)) % ALIGNMENT;
    if (total > SIZE_MAX - padding)
        return NULL;

    total += padding;

/* Ensure the request fits in sbrk's signed increment type. */
if (total > (size_t)INTPTR_MAX)
    return NULL;

/* Prevent heap byte counter overflow. */
if (heap_bytes > SIZE_MAX - total)
    return NULL;

/* Request the memory. */
void *memory = sbrk((intptr_t)total);
    if (memory == (void *)-1)
        return NULL;

    /* Create the new free block. */
    block_t *block = (block_t *)((char *)memory + padding);

    block->size = total - padding - HDR;
    block->next = NULL;
    block->free = 1;
    block->magic = BLOCK_MAGIC;

    /* Add the block to the end of the free list. */
    if (free_head == NULL) {
        free_head = block;
        free_tail = block;
    } else {
        free_tail->next = block;
        free_tail = block;
    }

    heap_bytes += total;

    return block;
}

/* Allocate memory using the first-fit placement policy. */
void *my_malloc(size_t size)
{
    /* A zero-size allocation returns NULL. */
    if (size == 0)
        return NULL;

    /* Align the requested size to 16 bytes. */
    size_t aligned_size = align_up(size);

    if (aligned_size == 0)
        return NULL;

    /* Search the free list using first-fit. */
    block_t *current = free_head;

    while (current != NULL) {
        if (current->free && current->size >= aligned_size) {

            /* Split if the remainder can hold a header and 16 bytes. */
            if (current->size - aligned_size >= HDR + ALIGNMENT) {

                block_t *remainder = (block_t *)(
                    (char *)payload(current) + aligned_size
                );

                remainder->size = current->size - aligned_size - HDR;
                remainder->next = current->next;
                remainder->free = 1;
                remainder->magic = BLOCK_MAGIC;

                current->size = aligned_size;
                current->next = remainder;

                if (free_tail == current)
                    free_tail = remainder;
            }

            current->free = 0;
            return payload(current);
        }

        current = current->next;
    }

    /* Grow the heap when no suitable free block exists. */
    block_t *new_block = grow(aligned_size);

    if (new_block == NULL)
        return NULL;

    /* The new block is now in the free list.
       Let the normal first-fit logic allocate and split it. */
    return my_malloc(size);
}

/* Merge physically adjacent free blocks. */
static void coalesce(void)
{
    block_t *current = free_head;

    while (current != NULL && current->next != NULL) {
        block_t *next = current->next;

        /* Merge only if both blocks are free and physically adjacent. */
        if (current->free && next->free &&
            (char *)payload(current) + current->size == (char *)next) {

            /* Absorb the next block and its header. */
            current->size += HDR + next->size;
            current->next = next->next;

            /* Update the tail if the removed block was the tail. */
            if (free_tail == next)
                free_tail = current;
        } else {
            current = current->next;
        }
    }
}

/* Release an allocated block. */
void my_free(void *ptr)
{
    /* Ignore NULL pointers. */
    if (ptr == NULL)
        return;

    /* Find the corresponding block header. */
    block_t *block = header(ptr);

    /* Verify that this header belongs to our allocator. */
    block_t *current = free_head;

    while (current != NULL && current != block) {
        current = current->next;
    }

    /* Reject pointers that do not belong to the allocator. */
    if (current == NULL || block->magic != BLOCK_MAGIC) {
        const char message[] = "my_free: invalid pointer\n";
        write(STDERR_FILENO, message, sizeof(message) - 1);
        return;
    }

    /* Detect double frees. */
    if (block->free) {
        const char message[] = "my_free: double free detected\n";
        write(STDERR_FILENO, message, sizeof(message) - 1);
        return;
    }

    /* Mark the block as free. */
    block->free = 1;

    /* Merge all physically adjacent free blocks. */
    coalesce();
}
