
#define _DEFAULT_SOURCE

#include <assert.h>
#include <stdio.h>

#include "myalloc.c"

int main(void)
{
    void *a = my_malloc(200);
    void *b = my_malloc(200);
    void *c = my_malloc(200);

    assert(a != NULL);
    assert(b != NULL);
    assert(c != NULL);

    printf("PASS: three allocations succeed\n");

    my_free(a);
    printf("PASS: first block freed\n");

    my_free(c);
    printf("PASS: third block freed\n");

    my_free(b);
    printf("PASS: second block freed\n");

    /* Count all free blocks. */
    size_t count = 0;
    block_t *current = free_head;

    while (current != NULL) {
        if (current->free)
            count++;

        current = current->next;
    }

    assert(count == 1);

    printf("PASS: adjacent free blocks coalesced\n");
    printf("PASS: exactly one free block remains\n");

    printf("\nTask 5 tests completed successfully.\n");

    return 0;
}
