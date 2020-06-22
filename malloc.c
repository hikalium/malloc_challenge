#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *mmap_from_system(size_t size);
void munmap_to_system(void *ptr, size_t size);

#define MY_MALLOC_NAME v01_malloc

#define CONCAT2(x, y) x##y
#define CONCAT(x, y) CONCAT2(x, y)
#define MY_MALLOC_INIT() CONCAT(MY_MALLOC_NAME, _init())
#define MY_MALLOC_ALLOC(size) CONCAT(MY_MALLOC_NAME, _alloc(size))
#define MY_MALLOC_FREE(ptr) CONCAT(MY_MALLOC_NAME, _free(ptr))
#define MY_MALLOC_FINALIZE(ptr) CONCAT(MY_MALLOC_NAME, _finalize())
#define MY_TEST() CONCAT(MY_MALLOC_NAME, _test())

#define MAX_ALLOC_SIZE 4000
#define PAGE_SIZE 4096ULL

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
int challenge_idx;
int malloc_count;
int free_count;
int current_resident_blocks;
int max_resident_blocks;
int num_of_alloc_for_size[MAX_ALLOC_SIZE + 1]; // 8 <= size <= 4000
void measure_malloc_init() {
  novice_malloc_init();
  malloc_count = 0;
  free_count = 0;
  current_resident_blocks = 0;
  max_resident_blocks = 0;
  for (int i = 0; i < MAX_ALLOC_SIZE + 1; i++) {
    num_of_alloc_for_size[i] = 0;
  }
}
void *measure_malloc_alloc(size_t size) {
  malloc_count++;
  current_resident_blocks++;
  if (max_resident_blocks < current_resident_blocks) {
    max_resident_blocks = current_resident_blocks;
  }
  assert(8 <= size && size <= MAX_ALLOC_SIZE);
  num_of_alloc_for_size[size]++;
  return novice_malloc_alloc(size);
}
void measure_malloc_free(void *ptr) {
  free_count++;
  current_resident_blocks--;
  novice_malloc_free(ptr);
}

void measure_malloc_finalize() {
  printf("Additional statistics:\n");
  printf("  malloc_count = %d\n", malloc_count);
  printf("  free_count = %d\n", free_count);
  printf("  max_resident_blocks = %d\n", max_resident_blocks);
  printf("  Alloc counts:(size, count)\n");
  for (int i = 0; i < MAX_ALLOC_SIZE + 1; i++) {
    if (!num_of_alloc_for_size[i])
      continue;
    printf("Challenge%d>   %d, %d\n", challenge_idx, i,
           num_of_alloc_for_size[i]);
  }
  challenge_idx++;
  novice_malloc_finalize();
}

//
// 01
//

typedef struct {
  int64_t used_bitmap;
  int next_slot_cursor;
} ChunkHeader;
typedef struct {
  // bs = 128
  // 4096 / 128 - 1 = 31 slots
  ChunkHeader **pages;
  int pages_used;
  int pages_capacity; // multiple of (PAGE_SIZE / sizeof(ChunkHeader*))
  int next_page_cursor;
} SlotAllocator128;

SlotAllocator128 malloc128;

void v01_malloc_init() {
  malloc128.pages = NULL;
  malloc128.pages_used = 0;
  malloc128.pages_capacity = 0;
  malloc128.next_page_cursor = 0;
}
static int find_empty_index(ChunkHeader *h) {
  for (int i = h->next_slot_cursor; i < sizeof(h->used_bitmap) * 4; i++) {
    if (((h->used_bitmap >> i) & 1) == 0) {
      h->next_slot_cursor = i;
      return i;
    }
  }
  h->next_slot_cursor = sizeof(h->used_bitmap) * 4;
  return -1;
}
static void *try_alloc128_from_page(ChunkHeader *h) {
  int empty_slot = find_empty_index(h);
  if (empty_slot == -1) {
    return NULL;
  }
  h->used_bitmap |= (1ULL << empty_slot);
  void *p = (void *)((uint8_t *)h + (empty_slot * 128));
  return p;
}
static ChunkHeader *alloc_page128() {
  ChunkHeader *h = mmap_from_system(PAGE_SIZE);
  // Clear to zero
  for (int i = 0; i < PAGE_SIZE / sizeof(uint64_t); i++) {
    ((uint64_t *)h)[i] = 0;
  }
  // Mark first slot is allocated (for metadata)
  h->used_bitmap = ~((1ULL << 32) - 2);
  return h;
}
static void *alloc128() {
  void *p;
  int empty_slot_idx = -1;
  for (int i = malloc128.next_page_cursor; i < malloc128.pages_used; i++) {
    if (empty_slot_idx == -1 && !malloc128.pages[i]) {
      empty_slot_idx = i;
      continue;
    }
    if ((p = try_alloc128_from_page(malloc128.pages[i]))) {
      malloc128.next_page_cursor = i;
      return p;
    }
  }
  if (empty_slot_idx == -1) {
    if (malloc128.pages_used == malloc128.pages_capacity) {
      // Expand page list
      const int new_capacity =
          malloc128.pages_capacity + PAGE_SIZE / sizeof(ChunkHeader *);
      ChunkHeader **new_pages128 =
          mmap_from_system(new_capacity * sizeof(ChunkHeader *));
      memcpy(new_pages128, malloc128.pages,
             sizeof(ChunkHeader *) * malloc128.pages_capacity);
      empty_slot_idx = malloc128.pages_capacity;
      bzero(&new_pages128[malloc128.pages_capacity],
            sizeof(ChunkHeader *) * (new_capacity - malloc128.pages_capacity));
      if (malloc128.pages) {
        munmap_to_system(malloc128.pages,
                         malloc128.pages_capacity * sizeof(ChunkHeader *));
      }
      malloc128.pages = new_pages128;
      malloc128.pages_capacity = new_capacity;
    }
    assert(malloc128.pages_used < malloc128.pages_capacity);
    empty_slot_idx = malloc128.pages_used;
    malloc128.pages_used++;
  }
  assert(!malloc128.pages[empty_slot_idx]);
  malloc128.pages[empty_slot_idx] = alloc_page128();
  malloc128.next_page_cursor = empty_slot_idx;
  return try_alloc128_from_page(malloc128.pages[empty_slot_idx]);
}
static void free_from_page128(ChunkHeader *h, void *ptr) {
  int slot = ((uint64_t)ptr & (PAGE_SIZE - 1)) >> 7;
  h->used_bitmap ^= (1ULL << slot);
  if (slot < h->next_slot_cursor) {
    h->next_slot_cursor = slot;
  }
}
static bool free128(void *ptr) {
  // retv: ptr is freed or not
  int idx = -1;
  ChunkHeader *key = (ChunkHeader *)((uint64_t)ptr & ~(PAGE_SIZE - 1));
  for (int i = 0; i < malloc128.pages_used; i++) {
    if (malloc128.pages[i] == key) {
      idx = i;
      break;
    }
  }
  if (idx == -1) {
    return false;
  }
  if (idx < malloc128.next_page_cursor) {
    malloc128.next_page_cursor = idx;
  }
  free_from_page128(malloc128.pages[idx], ptr);
  return true;
};
void *v01_malloc_alloc(size_t size) {
  if (size <= 128) {
    return alloc128();
  }
  return mmap_from_system(4096);
}
void v01_malloc_free(void *ptr) {
  if (free128(ptr)) {
    return;
  }
  munmap_to_system(ptr, 4096);
}
void v01_malloc_finalize() {}
void v01_malloc_test() {
  static void *arr[32];
  printf("%s begin\n", __func__);
  v01_malloc_init();
  for (int i = 0; i < 8; i++) {
    arr[i] = alloc128();
  }
  for (int i = 0; i < 8; i++) {
    free128(arr[i]);
  }
  for (int i = 0; i < 32; i++) {
    arr[i] = alloc128();
  }
  for (int i = 0; i < 32; i++) {
    free128(arr[i]);
  }
  v01_malloc_free(v01_malloc_alloc(256));
  // exit(EXIT_SUCCESS);
  printf("%s end\n", __func__);
}

//
// Wrapper
//

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
  MY_TEST();
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
