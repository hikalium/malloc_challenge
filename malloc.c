#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void *mmap_from_system(size_t size);
void munmap_to_system(void *ptr, size_t size);

#define MAX_ALLOC_SIZE 4000
#define PAGE_SIZE 4096ULL

typedef struct {
  int64_t used_bitmap;
  int next_slot_cursor;
} ChunkHeader;
typedef struct {
  // bs = 128
  // 4096 / 128 - 1 = 31 slots
  ChunkHeader **pages;
  int64_t pages_used;
  int64_t pages_capacity; // multiple of (PAGE_SIZE / sizeof(ChunkHeader*))
  int64_t next_page_cursor;
} SlotAllocator128;

SlotAllocator128 malloc128;

static int SA128_FindEmptyIndex(ChunkHeader *h) {
  for (int i = h->next_slot_cursor; i < sizeof(h->used_bitmap) * 4; i++) {
    if (((h->used_bitmap >> i) & 1) == 0) {
      h->next_slot_cursor = i;
      return i;
    }
  }
  h->next_slot_cursor = sizeof(h->used_bitmap) * 4;
  return -1;
}
static void *SA128_TryAllocFromPage(ChunkHeader *h) {
  int empty_slot = SA128_FindEmptyIndex(h);
  if (empty_slot == -1) {
    return NULL;
  }
  h->used_bitmap |= (1ULL << empty_slot);
  void *p = (void *)((uint8_t *)h + (empty_slot * 128));
  return p;
}
static ChunkHeader *SA128_AllocPage() {
  ChunkHeader *h = mmap_from_system(PAGE_SIZE);
  // Clear to zero
  for (int i = 0; i < PAGE_SIZE / sizeof(uint64_t); i++) {
    ((uint64_t *)h)[i] = 0;
  }
  // Mark first slot is allocated (for metadata)
  h->used_bitmap = ~((1ULL << 32) - 2);
  return h;
}
static void *SA128_Alloc() {
  void *p;
  int empty_slot_idx = -1;
  for (int i = malloc128.next_page_cursor; i < malloc128.pages_used; i++) {
    if (empty_slot_idx == -1 && !malloc128.pages[i]) {
      empty_slot_idx = i;
      continue;
    }
    if ((p = SA128_TryAllocFromPage(malloc128.pages[i]))) {
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
  malloc128.pages[empty_slot_idx] = SA128_AllocPage();
  malloc128.next_page_cursor = empty_slot_idx;
  return SA128_TryAllocFromPage(malloc128.pages[empty_slot_idx]);
}
static void SA128_FreeFromPage(ChunkHeader *h, void *ptr) {
  int slot = ((uint64_t)ptr & (PAGE_SIZE - 1)) >> 7;
  h->used_bitmap ^= (1ULL << slot);
  if (slot < h->next_slot_cursor) {
    h->next_slot_cursor = slot;
  }
}
static bool SA128_Free(void *ptr) {
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
  SA128_FreeFromPage(malloc128.pages[idx], ptr);
  return true;
};

//
// Interfaces
//

// This is called only once at the beginning of each challenge.
void my_initialize() {
  malloc128.pages = NULL;
  malloc128.pages_used = 0;
  malloc128.pages_capacity = 0;
  malloc128.next_page_cursor = 0;
}

// This is called every time an object is allocated. |size| is guaranteed
// to be a multiple of 8 bytes and meets 8 <= |size| <= 4000. You are not
// allowed to use any library functions other than mmap_from_system /
// munmap_to_system.
void *my_malloc(size_t size) {
  if (size <= 128) {
    return SA128_Alloc();
  }
  return mmap_from_system(4096);
}

// This is called every time an object is freed.  You are not allowed to use
// any library functions other than mmap_from_system / munmap_to_system.
void my_free(void *ptr) {
  if (SA128_Free(ptr)) {
    return;
  }
  munmap_to_system(ptr, 4096);
}

void my_finalize() {}

void test() {
  static void *arr[32];
  printf("%s begin\n", __func__);
  my_initialize();
  // SA128
  for (int i = 0; i < 8; i++) {
    arr[i] = SA128_Alloc();
  }
  for (int i = 0; i < 8; i++) {
    assert(SA128_Free(arr[i]));
  }
  for (int i = 0; i < 32; i++) {
    arr[i] = SA128_Alloc();
  }
  for (int i = 0; i < 32; i++) {
    SA128_Free(arr[i]);
  }
  // check other size
  my_free(my_malloc(256));
  // exit(EXIT_SUCCESS);
  my_finalize();
}
