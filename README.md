# malloc_challenge

- `malloc` is the malloc challenge. Please read this doc and [malloc/malloc.c](./malloc/malloc.c) for more information.
- `visualizer/` contains a visualizer of malloc traces.

## Instruction

Your task is implement a better malloc logic in [malloc.c](./malloc/malloc.c) to improve the speed and memory usage.

## How to build & run a benchmark

```
# clone this repo
git clone https://github.com/hikalium/malloc_challenge.git

# move into malloc dir
cd malloc_challenge
cd malloc

# build
make

# run a benchmark (for score board)
make run

# run a small benchmark for tracing (NOT for score board, just for visualization and debugging purpose)
make run_trace
```

If the commands above don't work, please make sure the following packages are installed:
```
# For Debian-based OS
sudo apt install make clang
```

Alternatively, you can build and run the challenge directly by running:

```
gcc -Wall -O3 -lm -o malloc_challenge.bin main.c malloc.c simple_malloc.c
./malloc_challenge.bin
```

## Acknowledgement

This work is based on [xharaken's malloc_challenge.c](https://github.com/xharaken/step2/blob/master/malloc_challenge.c). Thank you haraken-san!

