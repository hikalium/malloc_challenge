# Real malloc challenge!

## Instruction

Your task is implement a better malloc logic in [malloc.c](malloc.c) to improve the speed and memory usage.

## How to build & run a benchmark

```
# build
make
# run a benchmark
make run
```

If the commands above don't work, you can build and run the challenge directly by running:

```
gcc -Wall -O3 -lm -o malloc_challenge.bin main.c malloc.c simple_malloc.c
./malloc_challenge.bin
```

## Acknowledgement

This work is based on [xharaken's malloc_challenge.c](https://github.com/xharaken/step2/blob/master/malloc_challenge.c). Thank you haraken-san!
