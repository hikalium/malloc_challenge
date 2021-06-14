CFLAGS_COMMON=-Wall -g -lm
CFLAGS=-O3 $(CFLAGS_COMMON)
CFLAGS_ASAN=-O1 -fsanitize=address -fno-omit-frame-pointer $(CFLAGS_COMMON)
SRCS=main.c malloc.c simple_malloc.c

malloc_challenge.bin : ${SRCS} Makefile
	$(CC) -o $@ $(SRCS) $(CFLAGS)

malloc_challenge_with_trace.bin : ${SRCS} Makefile
	$(CC) -DENABLE_MALLOC_TRACE -o $@ $(SRCS) $(CFLAGS)

malloc_challenge_with_asan.bin : ${SRCS} Makefile
	$(CC) -DENABLE_MALLOC_TRACE -o $@ $(SRCS) $(CFLAGS_ASAN)

run : malloc_challenge.bin
	./malloc_challenge.bin

run_trace : malloc_challenge_with_trace.bin
	./malloc_challenge_with_trace.bin

run_valgrind : malloc_challenge_with_trace.bin
	valgrind ./malloc_challenge_with_trace.bin

run_asan : malloc_challenge_with_asan.bin
	./malloc_challenge_with_asan.bin

clean :
	-rm *.txt
	-rm *.bin

commit :
	clang-format -i *.c
	git add .
	git commit

dump_env :
	$(CC) --version
