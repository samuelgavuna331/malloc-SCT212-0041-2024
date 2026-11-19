#define _DEFAULT_SOURCE
#include <stdio.h>
#include "myalloc.c"

int main(void)
{
    int value = 10;

    void *p = my_malloc(32);
    void *q = my_malloc(32);

    if (p == NULL || q == NULL) {
        printf("FAIL: allocation failed\n");
        return 1;
    }

    printf("PASS: allocations succeeded\n");

    my_free(p);
    printf("PASS: first free succeeded\n");

    /* This should report a double free. */
    my_free(p);

    /* This should report an invalid pointer. */
    my_free(&value);

    my_free(q);

    printf("PASS: safety tests completed\n");

    return 0;
}
