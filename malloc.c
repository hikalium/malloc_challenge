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

#define MAX_NUM_OF_SLOTS 256

typedef struct {
  int64_t used_bitmap[MAX_NUM_OF_SLOTS / (sizeof(int64_t) * 8)];
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
void SAExpandPageListIfNeeded(SlotAllocator *sa) {
  if (sa->pages_used < sa->pages_capacity) {
    // Page list has a room for new page. Do nothing.
    return;
  }
  const int new_capacity =
      sa->pages_capacity + PAGE_SIZE / sizeof(ChunkHeader *);
  ChunkHeader **new_pages =
      mmap_from_system(new_capacity * sizeof(ChunkHeader *));
  memcpy(new_pages, sa->pages, sizeof(ChunkHeader *) * sa->pages_capacity);
  bzero(&new_pages[sa->pages_capacity],
        sizeof(ChunkHeader *) * (new_capacity - sa->pages_capacity));
  if (sa->pages) {
    munmap_to_system(sa->pages, sa->pages_capacity * sizeof(ChunkHeader *));
  }
  sa->pages = new_pages;
  sa->pages_capacity = new_capacity;
}
static int CHReadUsedBitmap(ChunkHeader *ch, int i) {
  return ((ch->used_bitmap[i / 64] >> (i % 64)) & 1);
}
static void CHSetUsedBitmap(ChunkHeader *ch, int i) {
  ch->used_bitmap[i / 64] |= (1ULL << (i % 64));
}
static void CHClearUsedBitmap(ChunkHeader *ch, int i) {
  ch->used_bitmap[i / 64] &= ~(1ULL << (i % 64));
}
static void CHSetAllUsedBitmap(ChunkHeader *ch) {
  memset(ch->used_bitmap, 0xFF, sizeof(ch->used_bitmap));
}
static int FindEmptyIndex(ChunkHeader *ch) {
  for (int i = ch->next_slot_cursor; i < sizeof(ch->used_bitmap) * 8; i++) {
    if (!CHReadUsedBitmap(ch, i)) {
      ch->next_slot_cursor = i;
      return i;
    }
  }
  ch->next_slot_cursor = sizeof(ch->used_bitmap) * 8;
  return -1;
}
static void *TryAllocFromPage(ChunkHeader *ch, int slot_size) {
  int empty_slot = FindEmptyIndex(ch);
  if (empty_slot == -1) {
    return NULL;
  }
  CHSetUsedBitmap(ch, empty_slot);
  void *p = (void *)((uint8_t *)ch + (empty_slot * slot_size));
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
static ChunkHeader *AllocPageForSlotAllocator(int slot_size, int slots,
                                              int slots_for_header) {
  // This function supports up to 64 slots
  assert(slots <= MAX_NUM_OF_SLOTS);
  assert(slots_for_header <= MAX_NUM_OF_SLOTS &&
         sizeof(ChunkHeader) <= (slot_size * slots_for_header));
  ChunkHeader *ch = mmap_from_system(PAGE_SIZE);
  // Clear to zero
  for (int i = 0; i < PAGE_SIZE / sizeof(uint64_t); i++) {
    ((uint64_t *)ch)[i] = 0;
  }
  // Mark first slot is allocated (for metadata)
  CHSetAllUsedBitmap(ch);
  for (int i = slots_for_header; i < slots; i++) {
    CHClearUsedBitmap(ch, i);
  }
  return ch;
}

#define SA16_LOG2_OF_SLOT_SIZE 4
#define SA16_SLOT_SIZE (1ULL << SA16_LOG2_OF_SLOT_SIZE)
#define SA16_NUM_OF_SLOTS (PAGE_SIZE >> SA16_LOG2_OF_SLOT_SIZE)
#define SA16_NUM_OF_SLOTS_RESERVED 3
static void *SA16_Alloc() {
  int empty_slot_idx = -1;
  void *p = TryAllocFromExistedPages(&sa16, &empty_slot_idx, 16);
  if (p) {
    // Found an empty slot in allocated pages.
    return p;
  }
  if (empty_slot_idx == -1) {
    SAExpandPageListIfNeeded(&sa16);
    assert(sa16.pages_used < sa16.pages_capacity);
    empty_slot_idx = sa16.pages_used;
    sa16.pages_used++;
  }
  assert(!sa16.pages[empty_slot_idx]);
  sa16.pages[empty_slot_idx] = AllocPageForSlotAllocator(
      SA16_SLOT_SIZE, SA16_NUM_OF_SLOTS, SA16_NUM_OF_SLOTS_RESERVED);
  sa16.next_page_cursor = empty_slot_idx;
  return TryAllocFromPage(sa16.pages[empty_slot_idx], SA16_SLOT_SIZE);
}

#define SA128_LOG2_OF_SLOT_SIZE 7
static void *SA128_Alloc() {
  int empty_slot_idx = -1;
  void *p = TryAllocFromExistedPages(&sa128, &empty_slot_idx, 128);
  if (p) {
    // Found an empty slot in allocated pages.
    return p;
  }
  if (empty_slot_idx == -1) {
    SAExpandPageListIfNeeded(&sa128);
    assert(sa128.pages_used < sa128.pages_capacity);
    empty_slot_idx = sa128.pages_used;
    sa128.pages_used++;
  }
  assert(!sa128.pages[empty_slot_idx]);
  sa128.pages[empty_slot_idx] = AllocPageForSlotAllocator(128, 32, 1);
  sa128.next_page_cursor = empty_slot_idx;
  return TryAllocFromPage(sa128.pages[empty_slot_idx], 128);
}
static void SAFreeFromPage(SlotAllocator *sa, int page_idx, void *ptr,
                           int log2_slot_size) {
  ChunkHeader *ch = sa->pages[page_idx];
  int slot = ((uint64_t)ptr & (PAGE_SIZE - 1)) >> log2_slot_size;
  CHClearUsedBitmap(ch, slot);
  if (slot < ch->next_slot_cursor) {
    ch->next_slot_cursor = slot;
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
  SAFreeFromPage(&sa128, idx, ptr, SA128_LOG2_OF_SLOT_SIZE);
  return true;
};
static bool SA16_Free(void *ptr) {
  // retv: ptr is freed or not
  int idx = -1;
  ChunkHeader *key = (ChunkHeader *)((uint64_t)ptr & ~(PAGE_SIZE - 1));
  for (int i = 0; i < sa16.pages_used; i++) {
    if (sa16.pages[i] == key) {
      idx = i;
      break;
    }
  }
  if (idx == -1) {
    return false;
  }
  if (idx < sa16.next_page_cursor) {
    sa16.next_page_cursor = idx;
  }
  SAFreeFromPage(&sa16, idx, ptr, SA16_LOG2_OF_SLOT_SIZE);
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
  if (size <= 16) {
    return SA16_Alloc();
  }
  if (size <= 128) {
    return SA128_Alloc();
  }
  return mmap_from_system(4096);
}

// This is called every time an object is freed.  You are not allowed to use
// any library functions other than mmap_from_system / munmap_to_system.
void my_free(void *ptr) {
  if (SA16_Free(ptr)) {
    return;
  }
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
  // SA16
  for (int i = 0; i < 8; i++) {
    arr[i] = SA16_Alloc();
  }
  for (int i = 0; i < 8; i++) {
    assert(SA16_Free(arr[i]));
  }
  for (int i = 0; i < 32; i++) {
    arr[i] = SA16_Alloc();
  }
  for (int i = 0; i < 32; i++) {
    SA16_Free(arr[i]);
  }
  // check other size
  my_free(my_malloc(256));
  // exit(EXIT_SUCCESS);
  my_finalize();
  printf("%s end\n", __func__);
}
