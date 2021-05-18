default: hook.so trace2timeline.bin

%.bin : %.cc Makefile
	g++ -Wall -Wpedantic -o $@ $*.cc

hook.so : hook.c Makefile
	gcc -o hook.so -fPIC -shared hook.c -ldl -D_GNU_SOURCE

.PHONY : run_git clean

run_git : hook.so
	LD_PRELOAD=./hook.so git status

clean :
	-rm *.so trace*.txt
