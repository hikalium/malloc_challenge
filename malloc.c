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
  int log2_of_slot_size;
  int slot_size;
  int num_of_slots;
  int num_of_slots_reserved;
} SlotAllocator;

void InitSlotAllocator(SlotAllocator *sa, int log2_of_slot_size) {
  sa->pages = NULL;
  sa->pages_used = 0;
  sa->pages_capacity = 0;
  sa->next_page_cursor = 0;
  sa->log2_of_slot_size = log2_of_slot_size;
  sa->slot_size = 1ULL << log2_of_slot_size;
  sa->num_of_slots = PAGE_SIZE >> log2_of_slot_size;
  sa->num_of_slots_reserved =
      (sizeof(ChunkHeader) + sa->slot_size - 1) / sa->slot_size;
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
static void *TryAllocFromExistedPages(SlotAllocator *sa, int *empty_slot_idx) {
  for (int i = sa->next_page_cursor; i < sa->pages_used; i++) {
    if (*empty_slot_idx == -1 && !sa->pages[i]) {
      *empty_slot_idx = i;
      continue;
    }
    void *p;
    if ((p = TryAllocFromPage(sa->pages[i], sa->slot_size))) {
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
static void SAFreeFromPage(SlotAllocator *sa, int page_idx, void *ptr) {
  ChunkHeader *ch = sa->pages[page_idx];
  int slot = ((uint64_t)ptr & (PAGE_SIZE - 1)) >> sa->log2_of_slot_size;
  CHClearUsedBitmap(ch, slot);
  if (slot < ch->next_slot_cursor) {
    ch->next_slot_cursor = slot;
  }
}
static int SAFindPageForPtr(SlotAllocator *sa, void *ptr) {
  // Returns index of sa.pages. -1 if not found.
  ChunkHeader *key = (ChunkHeader *)((uint64_t)ptr & ~(PAGE_SIZE - 1));
  for (int i = 0; i < sa->pages_used; i++) {
    if (sa->pages[i] != key)
      continue;
    return i;
  }
  return -1;
}
static bool SAFree(SlotAllocator *sa, void *ptr) {
  // retv: ptr is freed or not
  int idx = SAFindPageForPtr(sa, ptr);
  if (idx == -1) {
    return false;
  }
  if (idx < sa->next_page_cursor) {
    sa->next_page_cursor = idx;
  }
  SAFreeFromPage(sa, idx, ptr);
  return true;
}
static void *SAAlloc(SlotAllocator *sa) {
  int empty_slot_idx = -1;
  void *p = TryAllocFromExistedPages(sa, &empty_slot_idx);
  if (p) {
    // Found an empty slot in allocated pages.
    return p;
  }
  if (empty_slot_idx == -1) {
    SAExpandPageListIfNeeded(sa);
    assert(sa->pages_used < sa->pages_capacity);
    empty_slot_idx = sa->pages_used;
    sa->pages_used++;
  }
  assert(!sa->pages[empty_slot_idx]);
  sa->pages[empty_slot_idx] = AllocPageForSlotAllocator(
      sa->slot_size, sa->num_of_slots, sa->num_of_slots_reserved);
  sa->next_page_cursor = empty_slot_idx;
  return TryAllocFromPage(sa->pages[empty_slot_idx], sa->slot_size);
}

//
// Interfaces
//

static SlotAllocator sa16;
static SlotAllocator sa32;
static SlotAllocator sa64;
static SlotAllocator sa128;

#define SAAlloc16() SAAlloc(&sa16)
#define SAFree16(p) SAFree(&sa16, p)
#define SAAlloc32() SAAlloc(&sa32)
#define SAFree32(p) SAFree(&sa32, p)
#define SAAlloc64() SAAlloc(&sa64)
#define SAFree64(p) SAFree(&sa64, p)
#define SAAlloc128() SAAlloc(&sa128)
#define SAFree128(p) SAFree(&sa128, p)

// This is called only once at the beginning of each challenge.
void my_initialize() {
  InitSlotAllocator(&sa16, 4 /* = log_2(16) */);
  InitSlotAllocator(&sa32, 5 /* = log_2(32) */);
  InitSlotAllocator(&sa64, 6 /* = log_2(64) */);
  InitSlotAllocator(&sa128, 7 /* = log_2(128) */);
}

// This is called every time an object is allocated. |size| is guaranteed
// to be a multiple of 8 bytes and meets 8 <= |size| <= 4000. You are not
// allowed to use any library functions other than mmap_from_system /
// munmap_to_system.
void *my_malloc(size_t size) {
  if (size <= 16) {
    return SAAlloc16();
  }
  if (size <= 32) {
    return SAAlloc32();
  }
  if (size <= 64) {
    return SAAlloc64();
  }
  if (size <= 128) {
    return SAAlloc128();
  }
  return mmap_from_system(4096);
}

// This is called every time an object is freed.  You are not allowed to use
// any library functions other than mmap_from_system / munmap_to_system.
void my_free(void *ptr) {
  if (SAFree16(ptr) || SAFree32(ptr) || SAFree64(ptr) || SAFree128(ptr)) {
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
    arr[i] = SAAlloc128();
  }
  for (int i = 0; i < 8; i++) {
    assert(!SAFree16(arr[i]));
    assert(SAFree128(arr[i]));
  }
  for (int i = 0; i < 32; i++) {
    arr[i] = SAAlloc128();
  }
  for (int i = 0; i < 32; i++) {
    assert(!SAFree16(arr[i]));
    assert(SAFree128(arr[i]));
  }
  // SA16
  for (int i = 0; i < 8; i++) {
    arr[i] = SAAlloc16();
  }
  for (int i = 0; i < 8; i++) {
    assert(!SAFree128(arr[i]));
    assert(SAFree16(arr[i]));
  }
  for (int i = 0; i < 32; i++) {
    arr[i] = SAAlloc16();
  }
  for (int i = 0; i < 32; i++) {
    assert(!SAFree128(arr[i]));
    assert(SAFree16(arr[i]));
  }
  // check other size
  my_free(my_malloc(256));
  // exit(EXIT_SUCCESS);
  my_finalize();
  printf("%s end\n", __func__);
}
