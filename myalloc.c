
#ifndef _DEFAULT_SOURCE
#define _DEFAULT_SOURCE
#endif

#include <stddef.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include "myalloc.h"
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
/* Current memory placement policy. */
static fit_policy_t allocation_policy = FIT_FIRST;
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
/* Allocate memory for an array and initialize it to zero. */
void *my_calloc(size_t nmemb, size_t size)
{
    /* Prevent multiplication overflow. */
    if (nmemb != 0 && size > SIZE_MAX / nmemb)
        return NULL;

    size_t total = nmemb * size;

    /* Allocate the required memory. */
    void *ptr = my_malloc(total);

    if (ptr == NULL)
        return NULL;

    /* Initialize all allocated bytes to zero. */
    memset(ptr, 0, total);

    return ptr;
}

/* Allocate memory using the selected placement policy. */
void *my_malloc(size_t size)
{
    if (size == 0)
        return NULL;

    size_t aligned_size = align_up(size);

    if (aligned_size == 0)
        return NULL;

    block_t *current = free_head;
    block_t *chosen = NULL;

    /* Search for a suitable free block. */
    while (current != NULL) {
        if (current->free && current->size >= aligned_size) {

            /* First-fit selects the first suitable block. */
            if (allocation_policy == FIT_FIRST) {
                chosen = current;
                break;
            }

            /* Best-fit selects the smallest suitable block. */
            if (chosen == NULL || current->size < chosen->size)
                chosen = current;
        }

        current = current->next;
    }

    /* Allocate from the selected block. */
    if (chosen != NULL) {

        /* Split if enough space remains for a header and payload. */
        if (chosen->size - aligned_size >= HDR + ALIGNMENT) {

            block_t *remainder = (block_t *)(
                (char *)payload(chosen) + aligned_size
            );

            remainder->size = chosen->size - aligned_size - HDR;
            remainder->next = chosen->next;
            remainder->free = 1;
            remainder->magic = BLOCK_MAGIC;

            chosen->size = aligned_size;
            chosen->next = remainder;

            if (free_tail == chosen)
                free_tail = remainder;
        }

        chosen->free = 0;
        return payload(chosen);
    }

    /* Grow the heap if no suitable block exists. */
    if (grow(aligned_size) == NULL)
        return NULL;

    return my_malloc(size);
}
/* Change the memory placement policy. */
void set_policy(fit_policy_t p)
{
    if (p == FIT_FIRST || p == FIT_BEST)
        allocation_policy = p;
    else
        allocation_policy = FIT_FIRST;
}
/* Collect statistics about the allocator's current heap. */
void heap_stats(heap_stats_t *out)
{
    if (out == NULL)
        return;

    /* Initialize all statistics. */
    out->heap_bytes = heap_bytes;
    out->used_bytes = 0;
    out->free_bytes = 0;
    out->blocks = 0;
    out->free_blocks = 0;
    out->largest_free = 0;

    /* Traverse every block once. */
    block_t *current = free_head;

    while (current != NULL) {
        out->blocks++;

        if (current->free) {
            out->free_blocks++;
            out->free_bytes += current->size;

            if (current->size > out->largest_free)
                out->largest_free = current->size;
        } else {
            out->used_bytes += current->size;
        }

        current = current->next;
    }
}
/* Display information about every block in the heap. */
void heap_dump(void)
{
    block_t *current = free_head;
    size_t index = 0;
    char buffer[160];

    while (current != NULL) {
        int length = snprintf(
            buffer,
            sizeof(buffer),
            "Block %zu: address=%p, size=%zu, status=%s\n",
            index,
            (void *)current,
            current->size,
            current->free ? "FREE" : "USED"
        );

        if (length > 0) {
            size_t output_length = (size_t)length;

            if (output_length >= sizeof(buffer))
                output_length = sizeof(buffer) - 1;

            write(STDOUT_FILENO, buffer, output_length);
        }

        current = current->next;
        index++;
    }
}
/* Verify the integrity of the allocator's heap. */
int heap_check(void)
{
    block_t *current = free_head;
    block_t *previous = NULL;
    block_t *last = NULL;

    while (current != NULL) {

        /* Rule 1: Every block must have the correct magic number. */
        if (current->magic != BLOCK_MAGIC)
            return 0;

        /* Rule 2: Every payload must be 16-byte aligned. */
        if ((uintptr_t)payload(current) % ALIGNMENT != 0)
            return 0;

        /* Rule 3: Blocks must appear in ascending address order. */
        if (previous != NULL &&
            (uintptr_t)current <= (uintptr_t)previous)
            return 0;

        /* Rule 4: Adjacent free blocks must be coalesced. */
        if (previous != NULL &&
            previous->free && current->free &&
            (char *)payload(previous) + previous->size ==
            (char *)current)
            return 0;

        previous = current;
        last = current;
        current = current->next;
    }

    /* Verify that the tail points to the final block. */
    if (last != free_tail)
        return 0;

    /* An empty list must have both boundaries set to NULL. */
    if (free_head == NULL && free_tail != NULL)
        return 0;

    return 1;
}
/* Resize an existing allocation. */
void *my_realloc(void *ptr, size_t size)
{
    /* NULL behaves like a normal allocation. */
    if (ptr == NULL)
        return my_malloc(size);

    /* A zero-size request frees the block. */
    if (size == 0) {
        my_free(ptr);
        return NULL;
    }

    /* Align the requested size. */
    size_t aligned_size = align_up(size);

    if (aligned_size == 0)
        return NULL;

    /* Find the existing block. */
    block_t *block = header(ptr);
    block_t *current = free_head;

    while (current != NULL && current != block)
        current = current->next;

    /* Reject invalid or already freed pointers. */
    if (current == NULL || block->magic != BLOCK_MAGIC || block->free)
        return NULL;

    /* Reuse the existing block if it is large enough. */
    if (block->size >= aligned_size)
        return ptr;

    /* Try to expand into the next physically adjacent free block. */
    block_t *next = block->next;

    if (next != NULL &&
        next->free &&
        (char *)payload(block) + block->size == (char *)next &&
        next->size <= SIZE_MAX - HDR &&
        block->size <= SIZE_MAX - HDR - next->size) {

        size_t combined_size = block->size + HDR + next->size;

        if (combined_size >= aligned_size) {

            block->size = combined_size;
            block->next = next->next;

            if (free_tail == next)
                free_tail = block;

            /* Split any sufficiently large remaining space. */
            if (combined_size - aligned_size >= HDR + ALIGNMENT) {

                block_t *remainder = (block_t *)(
                    (char *)payload(block) + aligned_size
                );

                remainder->size = combined_size - aligned_size - HDR;
                remainder->next = block->next;
                remainder->free = 1;
                remainder->magic = BLOCK_MAGIC;

                block->size = aligned_size;
                block->next = remainder;

                if (free_tail == block)
                    free_tail = remainder;
            }

            return ptr;
        }
    }

    /* Allocate a new block if expansion in place is impossible. */
    void *new_ptr = my_malloc(size);

    if (new_ptr == NULL)
        return NULL;

    /* Copy the smaller of the old and new sizes. */
    size_t copy_size = block->size;

    if (copy_size > size)
        copy_size = size;

    memcpy(new_ptr, ptr, copy_size);

    /* Free the old allocation. */
    my_free(ptr);

    return new_ptr;
}
