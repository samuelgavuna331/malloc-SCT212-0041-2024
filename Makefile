CC     = gcc
CFLAGS = -std=c11 -Wall -Wextra -g -O0

test: myalloc.c test_myalloc.c myalloc.h
	$(CC) $(CFLAGS) -o test_myalloc myalloc.c test_myalloc.c
	./test_myalloc

clean:
	rm -f test_myalloc *.o
