//
// >>>> malloc challenge <<<<
//

// Your task is to improve utilization and speed of the following malloc
// implementation.

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *mmap_from_system(size_t size);
void munmap_to_system(void *ptr, size_t size);

//
// malloc interfaces
//
// DO NOT REMOVE/RENAME FOLLOWING FUNCTIONS!
// Of course, you can change the code in the functions!

// This is called at the beginning of each challenge.
void my_initialize() {
  // Implement here!
}

// my_malloc() is called every time an object is allocated.
// |size| is guaranteed to be a multiple of 8 bytes and meets 8 <= |size| <= 4000.
// You are not allowed to use any library functions other than
// mmap_from_system() / munmap_to_system().
void *my_malloc(size_t size) {
  // Implement here!
  return mmap_from_system(4096);
}

// my_free() is called every time an object is freed.  You are not allowed to
// use any library functions other than mmap_from_system / munmap_to_system.
void my_free(void *ptr) {
  // Implement here!
  munmap_to_system(ptr, 4096);
}

// This is called at the end of each challenge.
void my_finalize() {
  // Implement here!
}

void test() {
  // Implement here!
  assert(1 == 1); /* 1 is 1. That's always true! (You can remove this.) */
}
