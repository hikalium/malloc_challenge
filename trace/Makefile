default: hook.so trace2timeline.bin alloc_free_seq.bin

%.bin : %.cc Makefile
	g++ -Wall -Wpedantic -o $@ $*.cc

%.bin : %.c Makefile
	gcc -Wall -Wpedantic -o $@ $*.c

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
	LD_PRELOAD=./hook.so g++ -Wall -Wpedantic -o /dev/null trace2timeline.cc 
	ls -Artla trace*.txt | head -n 1

trace_dbg : hook.so
	-rm trace*.txt
	LD_PRELOAD=./hook.so LD_DEBUG=libs,files g++ -S -o /dev/null trace2timeline.cc
	ls -Artla trace*.txt | head -n 1
