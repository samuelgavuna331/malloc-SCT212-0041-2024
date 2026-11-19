# Memory Allocator – Systems Programming

**Student Name:** Samuel Gavuna  
**Student ID:** SCT212-0041/2024  


## 1. Project Description

This project implements a simple dynamic memory allocator in C. It provides functions for allocating, releasing and managing memory without relying directly on the standard malloc() implementation for its custom allocation operations.

The allocator uses sbrk() to request memory from the operating system. It supports first-fit and best-fit allocation policies, block splitting, coalescing, and heap statistics.

## 2. Building and Running the Project

The project requires GCC and Make.

To build and run the test program:

```bash
make
```

Alternatively, run the tests directly using:


make test


To run the memory allocator benchmark:


make bench


To remove generated binaries and object files:

```bash
make clean
```

## 3. Task Completion

| Task | Description | Status |
|---|---|---|
| 1 | Block header and alignment | Complete |
| 2 | Heap growth using `sbrk()` | Complete |
| 3 | First-fit allocation | Complete |
| 4 | Block splitting | Complete |
| 5 | Freeing memory and coalescing | Complete |
| 6 | `calloc()` and `realloc()` | Complete |
| 7 | Best-fit allocation and heap diagnostics | Complete |
| 8 | Benchmark and performance report | Complete |

## 4. Design Explanation

My memory allocator uses a linked list to keep track of memory blocks obtained from the operating system. Each block has a header containing four fields: size, next, free, and magic. The size field stores the amount of payload memory available in the block, while the next field points to the following block. The free field shows whether a block is available for allocation, and the magic field helps identify valid block headers.

I used 16-byte alignment for both the header and payload sizes. This ensures that the memory returned to a program is properly aligned. The header size is rounded up using the HDR macro, while the align_up() function adjusts requested allocation sizes.

When a program requests memory, my_malloc() searches the linked list for a suitable free block. The allocator supports two placement policies: first-fit and best-fit. First-fit selects the first block that can satisfy the request, while best-fit searches for the smallest suitable block. If a block has enough extra space, it is split into an allocated block and a smaller free block.

When memory is released using my_free(), the block is marked as free. The coalesce() function checks neighbouring blocks and combines them only when they are physically adjacent in memory. This helps reduce fragmentation and makes larger free blocks available for future allocations.

When there is not enough free memory, the allocator uses sbrk() to request more space in chunks of at least 64 KB.

## 5. Test Results

The provided test program was built and executed successfully using `make test`.

The tests covered basic allocation, memory alignment, overlapping allocations, memory reuse, block splitting, coalescing, calloc(), realloc(), heap growth, placement policies and randomized stress testing.

Final test output:


T8 randomized stress test
  PASS  no data corruption during 20000 operations
  PASS  everything freed
  PASS  fully coalesced (few free blocks remain)
  PASS  heap consistent

Result: 29 passed, 0 failed

## 6. Questions

1. Why must the header size be a multiple of 16 as well as the payload sizes?

The header size must be a multiple of 16 so that the payload starts at a properly aligned address. The payload sizes are also rounded up to multiples of 16 to maintain alignment between blocks. This helps prevent memory alignment problems.

2. Why can `printf()` inside `my_malloc()` cause infinite recursion?

printf() may internally request memory using malloc(). If that eventually calls our custom allocator, my_malloc() can be called again while it is still handling the first request. This can lead to repeated calls and eventually cause a stack overflow.

3. Your search is O(n). Which extra field would let you find a block's previous neighbour in the list, and what is the name of the technique?

I would add a prev pointer to the block header so that each block can point to the previous block. This would change the list into a doubly linked list. It would make moving to the previous block faster, although searching through the list would still take O(n).

4. Explain the difference between internal and external fragmentation, and name one place in your allocator where each one comes from.

Internal fragmentation happens when an allocated block contains some unused space, such as when a requested size is rounded up to the next multiple of 16. External fragmentation happens when free memory is separated into smaller blocks. In our allocator, this can happen when different blocks are allocated and freed in different orders.

5. Why does `sbrk()` memory never get returned to the OS in your allocator? Describe how you could give back memory at the end of the heap.

Our allocator keeps released blocks in its linked list so that they can be reused for future allocations. It does not currently reduce the program break to return memory to the operating system. One way to improve it would be to check whether the last block in the heap is free and use sbrk() with a negative increment to release that memory.

6. Two blocks are consecutive in your list but not adjacent in memory. How did this happen, and what does your coalescing code do about it?

This can happen when memory is obtained from the operating system at different times, with a gap or other allocation between the regions. The blocks may be next to each other in the linked list even though their addresses are not next to each other. Our `coalesce()` function checks their actual memory addresses before merging them, so it only combines blocks that are physically adjacent.

## 7. Known Limitations

- Memory obtained using sbrk() is not currently returned to the operating system, even when blocks are freed.
- The allocator is designed for a single-threaded environment and does not provide thread synchronization.
- Searching for a suitable free block requires traversing the linked list, so allocation search time can increase as the number of blocks grows.
- The allocator is intended for the assignment and is not a replacement for a production memory allocator.

## 8. Sources and AI Assistance Declaration

The implementation was developed using the assignment instructions and the provided project files. I also referred to C language and Linux system documentation when working with memory allocation, pointer arithmetic and sbrk().

ChatGPT was used as an assistance tool for understanding some concepts, debugging errors, reviewing implementation steps and improving the project documentation. I reviewed the explanations, worked through the implementation, and ran the tests to verify the results.
