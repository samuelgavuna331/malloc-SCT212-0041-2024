CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -g -O0

test: myalloc.c test_myalloc.c myalloc.h
	$(CC) $(CFLAGS) -o test_myalloc myalloc.c test_myalloc.c
	./test_myalloc

bench: bench.c myalloc.c myalloc.h
	$(CC) $(CFLAGS) -o bench bench.c myalloc.c
	./bench

clean:
	rm -f test_myalloc bench *.o
