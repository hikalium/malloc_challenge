#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

void *mmap_from_system(size_t size);
void munmap_to_system(void *ptr, size_t size);

#define MY_MALLOC_NAME measure_malloc

#define CONCAT2(x, y) x##y
#define CONCAT(x, y) CONCAT2(x, y)
#define MY_MALLOC_INIT() CONCAT(MY_MALLOC_NAME, _init())
#define MY_MALLOC_ALLOC(size) CONCAT(MY_MALLOC_NAME, _alloc(size))
#define MY_MALLOC_FREE(ptr) CONCAT(MY_MALLOC_NAME, _free(ptr))
#define MY_MALLOC_FINALIZE(ptr) CONCAT(MY_MALLOC_NAME, _finalize())

//
// novice malloc
//
/*
Challenge 1: simple malloc => my malloc
Time: 13 ms => 405 ms
Utilization: 70% => 3%
==================================
Challenge 2: simple malloc => my malloc
Time: 9 ms => 400 ms
Utilization: 40% => 0%
==================================
Challenge 3: simple malloc => my malloc
Time: 108 ms => 396 ms
Utilization: 8% => 0%
==================================
Challenge 4: simple malloc => my malloc
Time: 6121 ms => 411 ms
Utilization: 15% => 21%
==================================
Challenge 5: simple malloc => my malloc
Time: 4452 ms => 412 ms
Utilization: 15% => 16%
==================================
*/
void novice_malloc_init() {}
void *novice_malloc_alloc(size_t size) { return mmap_from_system(4096); }
void novice_malloc_free(void *ptr) { munmap_to_system(ptr, 4096); }
void novice_malloc_finalize() {}

//
// novice malloc with measurement
//
int malloc_count;
int free_count;
void measure_malloc_init() {
  novice_malloc_init();
  malloc_count = 0;
  free_count = 0;
}
void *measure_malloc_alloc(size_t size) {
  malloc_count++;
  return novice_malloc_alloc(size);
}
void measure_malloc_free(void *ptr) {
  free_count++;
  novice_malloc_free(ptr);
}

void measure_malloc_finalize() {
  printf("Additional statistics:\n");
  printf("  malloc_count = %d\n", malloc_count);
  printf("  free_count = %d\n", free_count);
  novice_malloc_finalize();
}

// This is called only once at the beginning of each challenge.
void my_initialize() { MY_MALLOC_INIT(); }

// This is called every time an object is allocated. |size| is guaranteed
// to be a multiple of 8 bytes and meets 8 <= |size| <= 4000. You are not
// allowed to use any library functions other than mmap_from_system /
// munmap_to_system.
void *my_malloc(size_t size) { return MY_MALLOC_ALLOC(size); }

// This is called every time an object is freed.  You are not allowed to use
// any library functions other than mmap_from_system / munmap_to_system.
void my_free(void *ptr) { MY_MALLOC_FREE(ptr); }

void my_finalize() { MY_MALLOC_FINALIZE(); }

void test() {
  my_initialize();
  for (int i = 0; i < 100; i++) {
    void *ptr = my_malloc(96);
    my_free(ptr);
  }
  void *ptrs[100];
  for (int i = 0; i < 100; i++) {
    ptrs[i] = my_malloc(96);
  }
  for (int i = 0; i < 100; i++) {
    my_free(ptrs[i]);
  }
  my_finalize();
}
