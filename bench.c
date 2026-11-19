#define _DEFAULT_SOURCE

#include "myalloc.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdint.h>

#define OPERATIONS 50000
#define SLOT_COUNT 1024
#define RANDOM_SEED 20261119

typedef struct {
    size_t slot;
    size_t size;
    int allocate;
} operation_t;

/* Shared workload used by all three benchmark runs. */
static operation_t operations[OPERATIONS];

/* Generate a repeatable sequence of allocation and free operations. */
static void generate_workload(void)
{
    unsigned char occupied[SLOT_COUNT] = {0};

    srand(RANDOM_SEED);

    for (size_t i = 0; i < OPERATIONS; i++) {
        size_t slot = (size_t)rand() % SLOT_COUNT;

        operations[i].slot = slot;

        if (occupied[slot]) {
            operations[i].allocate = 0;
            operations[i].size = 0;
            occupied[slot] = 0;
        } else {
            operations[i].allocate = 1;
            operations[i].size = (size_t)(rand() % 4096) + 1;
            occupied[slot] = 1;
        }
    }
}

/* Calculate elapsed time in seconds. */
static double elapsed_seconds(struct timespec start,
                              struct timespec end)
{
    return (double)(end.tv_sec - start.tv_sec) +
           (double)(end.tv_nsec - start.tv_nsec) / 1000000000.0;
}

/* Run the custom allocator using the selected placement policy. */
static void benchmark_custom(fit_policy_t policy, const char *name)
{
    void *slots[SLOT_COUNT] = {0};

    set_policy(policy);

    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (size_t i = 0; i < OPERATIONS; i++) {
        size_t slot = operations[i].slot;

        if (operations[i].allocate) {
            slots[slot] = my_malloc(operations[i].size);

            if (slots[slot] != NULL) {
                volatile unsigned char *memory =
                    (volatile unsigned char *)slots[slot];

                memory[0] = (unsigned char)i;
                memory[operations[i].size - 1] = (unsigned char)(i >> 8);
            }
        } else {
            my_free(slots[slot]);
            slots[slot] = NULL;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    /* Capture statistics before releasing remaining allocations. */
    heap_stats_t stats;
    heap_stats(&stats);

    double fragmentation = 0.0;

    if (stats.free_bytes > 0) {
        fragmentation = 1.0 -
            (double)stats.largest_free / (double)stats.free_bytes;
    }

    int valid_before_cleanup = heap_check();

    /* Release all remaining allocations. */
    for (size_t i = 0; i < SLOT_COUNT; i++) {
        my_free(slots[i]);
    }

    int valid_after_cleanup = heap_check();

    printf("\n=== %s ===\n", name);
    printf("Operations:       %d\n", OPERATIONS);
    printf("Execution time:   %.6f seconds\n",
           elapsed_seconds(start, end));
    printf("Heap bytes:       %zu\n", stats.heap_bytes);
    printf("Free bytes:       %zu\n", stats.free_bytes);
    printf("Free blocks:      %zu\n", stats.free_blocks);
    printf("Largest free:     %zu\n", stats.largest_free);
    printf("Fragmentation:    %.4f (%.2f%%)\n",
           fragmentation, fragmentation * 100.0);
    printf("Heap valid:       %s\n",
           valid_before_cleanup ? "YES" : "NO");
    printf("Cleanup valid:    %s\n",
           valid_after_cleanup ? "YES" : "NO");

    if (!valid_before_cleanup || !valid_after_cleanup)
        exit(EXIT_FAILURE);
}

/* Run the same workload using the standard C allocator. */
static void benchmark_libc(void)
{
    void *slots[SLOT_COUNT] = {0};

    struct timespec start, end;

    clock_gettime(CLOCK_MONOTONIC, &start);

    for (size_t i = 0; i < OPERATIONS; i++) {
        size_t slot = operations[i].slot;

        if (operations[i].allocate) {
            slots[slot] = malloc(operations[i].size);

            if (slots[slot] != NULL) {
                volatile unsigned char *memory =
                    (volatile unsigned char *)slots[slot];

                memory[0] = (unsigned char)i;
                memory[operations[i].size - 1] = (unsigned char)(i >> 8);
            }
        } else {
            free(slots[slot]);
            slots[slot] = NULL;
        }
    }

    clock_gettime(CLOCK_MONOTONIC, &end);

    double elapsed = elapsed_seconds(start, end);

    for (size_t i = 0; i < SLOT_COUNT; i++)
        free(slots[i]);

    printf("\n=== Standard C malloc/free ===\n");
    printf("Operations:       %d\n", OPERATIONS);
    printf("Execution time:   %.6f seconds\n", elapsed);
    printf("Heap statistics:  Not measured by this benchmark\n");
}

/* Run each benchmark in a separate process. */
static int run_isolated(int benchmark)
{
    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        return -1;
    }

    if (pid == 0) {
        if (benchmark == 0)
            benchmark_custom(FIT_FIRST, "FIRST-FIT");
        else if (benchmark == 1)
            benchmark_custom(FIT_BEST, "BEST-FIT");
        else
            benchmark_libc();

        fflush(stdout);
        _exit(EXIT_SUCCESS);
    }

    int status;

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        return -1;
    }

    return WIFEXITED(status) && WEXITSTATUS(status) == 0 ? 0 : -1;
}

int main(void)
{
    printf("Memory Allocator Benchmark\n");
    printf("==========================\n");
    printf("Operations: %d\n", OPERATIONS);
    printf("Allocation sizes: 1 - 4096 bytes\n");
    printf("Random seed: %d\n", RANDOM_SEED);

    generate_workload();

    fflush(stdout);

    if (run_isolated(0) != 0)
        return EXIT_FAILURE;

    if (run_isolated(1) != 0)
        return EXIT_FAILURE;

    if (run_isolated(2) != 0)
        return EXIT_FAILURE;

    printf("\nBenchmark completed successfully.\n");

    return EXIT_SUCCESS;
}
