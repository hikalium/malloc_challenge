default: hook.so trace2timeline.bin alloc_free_seq.bin

%.png : %_gnuplot.txt %.dat Makefile
	gnuplot -c $*_gnuplot.txt

%.bin : %.cc Makefile
	g++ -Wall -Wpedantic -o $@ $*.cc

%.bin : %.c Makefile
	gcc -Wall -Wpedantic -static -o $@ $*.c

%.dat : %.txt trace2timeline.bin Makefile
	cat $*.txt | ./trace2timeline.bin > $@

hook.so : hook.c Makefile
	gcc -o hook.so -fPIC -shared hook.c -ldl -D_GNU_SOURCE

.PHONY : run_git clean

run_git : hook.so
	LD_PRELOAD=./hook.so git status

clean :
	-rm trace*.txt

distclean :
	make clean
	-rm *.so *.bin

trace : hook.so
	-rm trace*.txt
	LD_PRELOAD=./hook.so g++ -S -o /dev/null trace2timeline.cc
	ls -Artla trace*.txt | head -n 1

trace2 : hook.so
	-rm trace*.txt
	LD_PRELOAD=./hook.so gcc -S -o /dev/null hello_world.c 
	ls -Artla trace*.txt | head -n 1

trace3 : hook.so
	-rm trace*.txt
	LD_PRELOAD=./hook.so bash -c "echo hello"
	ls -Artla trace*.txt | head -n 1
	mv trace*.txt trace3_bash_hello.txt

trace4 : hook.so
	-rm trace*.txt
	LD_PRELOAD=./hook.so bash -c 'for i in {1..100} ; do echo $$i ; done'
	ls -Artla trace*.txt | head -n 1
	mv trace*.txt trace4_bash_loop.txt

trace5 : hook.so
	# https://www.reddit.com/r/bash/comments/6rs6sr/writing_fizzbuzz_in_bash/
	-rm trace*.txt
	LD_PRELOAD=./hook.so bash -c 'for ((i=1;i<=100;i++)); do if ! ((i%15)); then echo FizzBuzz; elif ! ((i%3)); then echo Fizz; elif ! ((i%5)); then echo Buzz; else echo $$i; fi; done'
	ls -Artla trace*.txt | head -n 1
	mv trace*.txt trace5_bash_fizzbuzz.txt

trace_dbg : hook.so
	-rm trace*.txt
	LD_PRELOAD=./hook.so LD_DEBUG=libs,files g++ -S -o /dev/null trace2timeline.cc
	ls -Artla trace*.txt | head -n 1
