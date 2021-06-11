CFLAGS=-Wall -O3 -lm
SRCS=main.c malloc.c simple_malloc.c

malloc_challenge.bin : ${SRCS} Makefile
	$(CC) $(CFLAGS) -o $@ $(SRCS)

run : malloc_challenge.bin
	./malloc_challenge.bin

clean :
	-rm malloc_challenge.bin

commit :
	clang-format -i *.c
	git add .
	git commit

