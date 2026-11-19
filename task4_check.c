#define _DEFAULT_SOURCE

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "myalloc.c"

int main(void)
{
    /* Allocate from a newly grown 64 KB block. */
    void *a = my_malloc(4000);

    assert(a != NULL);
    printf("PASS: initial allocation succeeds\n");

    assert((uintptr_t)a % 16 == 0);
    printf("PASS: allocated pointer is 16-byte aligned\n");

    block_t *first = header(a);

    assert(first->size == 4000);
    printf("PASS: allocated block has the requested aligned size\n");

    /* Verify that the original block was split. */
    block_t *remainder = first->next;

    assert(remainder != NULL);
    assert(remainder->free == 1);
    printf("PASS: remainder is a free block\n");

    assert(remainder->size > 3000);
    printf("PASS: remainder contains substantial reusable memory\n");

    assert(free_tail == remainder);
    printf("PASS: free-list tail updated after splitting\n");

    /* Allocate again from the remaining free block. */
    void *b = my_malloc(64);

    assert(b != NULL);
    assert((uintptr_t)b % 16 == 0);
    printf("PASS: second allocation succeeds and is aligned\n");

    assert(b == payload(remainder));
    printf("PASS: second allocation reuses the remainder\n");

    /* Verify that the second allocation also splits its block. */
    block_t *second = header(b);

    assert(second->size == 64);
    assert(second->next != NULL);
    assert(second->next->free == 1);
    printf("PASS: second block was split correctly\n");

    /* Check that the blocks are physically adjacent. */
    assert((char *)payload(first) + first->size ==
           (char *)second);
    printf("PASS: split blocks are physically adjacent\n");

    assert(heap_bytes == 65536);
    printf("PASS: splitting did not grow the heap\n");

    printf("\nTask 4 tests completed successfully.\n");
    printf("First block: %zu bytes\n", first->size);
    printf("Second block: %zu bytes\n", second->size);
    printf("Remaining free block: %zu bytes\n", second->next->size);

    return 0;
}
