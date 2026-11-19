#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "myalloc.c"

int main(void)
{
    /* Test 1: calloc allocation and zero initialization. */
    int *numbers = my_calloc(5, sizeof(int));

    assert(numbers != NULL);

    for (int i = 0; i < 5; i++)
        assert(numbers[i] == 0);

    printf("PASS: calloc allocates zero-initialized memory\n");

    /* Test 2: calloc multiplication overflow. */
    void *overflow = my_calloc(SIZE_MAX, 2);

    assert(overflow == NULL);

    printf("PASS: calloc detects multiplication overflow\n");

    /* Test 3: realloc with NULL behaves like malloc. */
    char *text = my_realloc(NULL, 32);

    assert(text != NULL);

    strcpy(text, "Hello allocator");

    printf("PASS: realloc with NULL allocates memory\n");

    /* Test 4: realloc grows a block and preserves data. */
    char *larger = my_realloc(text, 128);

    assert(larger != NULL);
    assert(strcmp(larger, "Hello allocator") == 0);

    printf("PASS: realloc grows allocation and preserves data\n");

    /* Test 5: realloc with zero frees the allocation. */
    void *result = my_realloc(larger, 0);

    assert(result == NULL);

    printf("PASS: realloc with zero frees memory\n");

    /* Test 6: realloc reuses an existing sufficiently large block. */
    char *small = my_malloc(128);

    assert(small != NULL);

    block_t *small_header = header(small);

    char *same = my_realloc(small, 64);

    assert(same == small);
    assert(header(same) == small_header);

    printf("PASS: realloc reuses sufficiently large block\n");

    /* Test 7: realloc absorbs the next adjacent free block. */
    char *first = my_malloc(64);
    void *second = my_malloc(64);

    assert(first != NULL && second != NULL);

    strcpy(first, "Adjacent block test");

    block_t *first_header = header(first);

    my_free(second);

    char *expanded = my_realloc(first, 160);

    assert(expanded == first);
    assert(header(expanded) == first_header);
    assert(strcmp(expanded, "Adjacent block test") == 0);

    printf("PASS: realloc expands into adjacent free block\n");
    /* Test 8: realloc moves data when adjacent expansion is impossible. */
    char *moving = my_malloc(32);
    char *blocker = my_malloc(32);

    assert(moving != NULL && blocker != NULL);

    strcpy(moving, "Moving allocation test");
    strcpy(blocker, "Blocker remains intact");

    char *moved = my_realloc(moving, 256);

    assert(moved != NULL);
    assert(moved != moving);
    assert(strcmp(moved, "Moving allocation test") == 0);
    assert(strcmp(blocker, "Blocker remains intact") == 0);

    printf("PASS: realloc moves allocation and preserves data\n");

    my_free(moved);
    my_free(blocker);
    /* Test 9: free remaining allocations. */
    my_free(numbers);
    my_free(small);
    my_free(expanded);

    printf("PASS: remaining allocations freed\n");
    printf("\nTask 6 tests completed successfully.\n");

    return 0;
}
