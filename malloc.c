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
} SlotAllocator;

SlotAllocator sa16;
SlotAllocator sa128;

void InitSlotAllocator(SlotAllocator *sa) {
  sa->pages = NULL;
  sa->pages_used = 0;
  sa->pages_capacity = 0;
  sa->next_page_cursor = 0;
}
static int FindEmptyIndex(ChunkHeader *h) {
  // This function supports up to 64 slots
  for (int i = h->next_slot_cursor; i < sizeof(h->used_bitmap) * 8; i++) {
    if (((h->used_bitmap >> i) & 1) == 0) {
      h->next_slot_cursor = i;
      return i;
    }
  }
  h->next_slot_cursor = sizeof(h->used_bitmap) * 4;
  return -1;
}
static void *TryAllocFromPage(ChunkHeader *h, int slot_size) {
  int empty_slot = FindEmptyIndex(h);
  if (empty_slot == -1) {
    return NULL;
  }
  h->used_bitmap |= (1ULL << empty_slot);
  void *p = (void *)((uint8_t *)h + (empty_slot * slot_size));
  return p;
}
static void *TryAllocFromExistedPages(SlotAllocator *sa, int *empty_slot_idx,
                                      int slot_size) {
  for (int i = sa->next_page_cursor; i < sa->pages_used; i++) {
    if (*empty_slot_idx == -1 && !sa->pages[i]) {
      *empty_slot_idx = i;
      continue;
    }
    void *p;
    if ((p = TryAllocFromPage(sa->pages[i], slot_size))) {
      sa->next_page_cursor = i;
      return p;
    }
  }
  return NULL;
}
static ChunkHeader *AllocPageForSlotAllocator(int slots, int slots_for_header) {
  // This function supports up to 64 slots
  ChunkHeader *h = mmap_from_system(PAGE_SIZE);
  // Clear to zero
  for (int i = 0; i < PAGE_SIZE / sizeof(uint64_t); i++) {
    ((uint64_t *)h)[i] = 0;
  }
  // Mark first slot is allocated (for metadata)
  h->used_bitmap = ~(((1ULL << slots) - 1) ^ ((1ULL << slots_for_header) - 1));
  return h;
}
static void *SA128_Alloc() {
  int empty_slot_idx = -1;
  void *p = TryAllocFromExistedPages(&sa128, &empty_slot_idx, 128);
  if (p) {
    // Found an empty slot in allocated pages.
    return p;
  }
  if (empty_slot_idx == -1) {
    if (sa128.pages_used == sa128.pages_capacity) {
      // Expand page list
      const int new_capacity =
          sa128.pages_capacity + PAGE_SIZE / sizeof(ChunkHeader *);
      ChunkHeader **new_pages128 =
          mmap_from_system(new_capacity * sizeof(ChunkHeader *));
      memcpy(new_pages128, sa128.pages,
             sizeof(ChunkHeader *) * sa128.pages_capacity);
      empty_slot_idx = sa128.pages_capacity;
      bzero(&new_pages128[sa128.pages_capacity],
            sizeof(ChunkHeader *) * (new_capacity - sa128.pages_capacity));
      if (sa128.pages) {
        munmap_to_system(sa128.pages,
                         sa128.pages_capacity * sizeof(ChunkHeader *));
      }
      sa128.pages = new_pages128;
      sa128.pages_capacity = new_capacity;
    }
    assert(sa128.pages_used < sa128.pages_capacity);
    empty_slot_idx = sa128.pages_used;
    sa128.pages_used++;
  }
  assert(!sa128.pages[empty_slot_idx]);
  sa128.pages[empty_slot_idx] = AllocPageForSlotAllocator(32, 1);
  sa128.next_page_cursor = empty_slot_idx;
  return TryAllocFromPage(sa128.pages[empty_slot_idx], 128);
}
static void SA128_FreeFromPage(int page_idx, void *ptr) {
  ChunkHeader *h = sa128.pages[page_idx];
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
  for (int i = 0; i < sa128.pages_used; i++) {
    if (sa128.pages[i] == key) {
      idx = i;
      break;
    }
  }
  if (idx == -1) {
    return false;
  }
  if (idx < sa128.next_page_cursor) {
    sa128.next_page_cursor = idx;
  }
  SA128_FreeFromPage(idx, ptr);
  return true;
};

//
// Interfaces
//

// This is called only once at the beginning of each challenge.
void my_initialize() {
  InitSlotAllocator(&sa16);
  InitSlotAllocator(&sa128);
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
