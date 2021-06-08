// from: https://github.com/xharaken/step2/blob/master/malloc_challenge.c

////////////////////////////////////////////////////////////////////////////////
/*                 (๑＞◡＜๑)  Malloc Challenge!!  (◍＞◡＜◍)                   */
////////////////////////////////////////////////////////////////////////////////

//
// Welcome to Malloc Challenge!! Your job is to invent a smart malloc algorithm.
//
// Rules:
//
// 1. Your job is to implement my_malloc(), my_free() and my_initialize().
//   *  my_initialize() is called only once at the beginning of each challenge.
//      You can initialize the memory allocator.
//   *  my_malloc(size) is called every time an object is allocated. In this
//      challenge, |size| is guaranteed to be a multiple of 8 bytes and meets
//      8 <= size <= 4000.
//   * my_free(ptr) is called every time an object is freed.
// 2. The only library functions you can use in my_malloc() and my_free() are
//    mmap_from_system() and munmap_to_system().
//   *  mmap_from_system(size) allocates |size| bytes from the system. |size|
//      needs to be a multiple of 4096 bytes. mmap_from_system(size) is a
//      system call and heavy. You are expected to minimize the call of
//      mmap_from_system(size) by reusing the returned
//      memory region as much as possible.
//   *  munmap_to_system(ptr, size) frees the memory region [ptr, ptr + size)
//      to the system. |ptr| and |size| need to be a multiple of 4096 bytes.
//      You are expected to free memory regions that are unused.
//   *  You are NOT allowed to use any other library functions at all, including
//      the default malloc() / free(), std:: libraries etc. This is because you
//      are implementing malloc itself -- if you use something that may use
//      malloc internally, it will result in an infinite recurion.
// 3. simple_malloc(), simple_free() and simple_initialize() are an example,
//    straightforward implementation. Your job is to invent a smarter malloc
//    algorithm than the simple malloc.
// 4. There are five challenges (Challenge 1, 2, 3, 4 and 5). Each challenge
//    allocates and frees many objects with different patterns. Your malloc
//    is evaluated by two criteria.
//   *  [Speed] How faster your malloc finishes the challange compared to
//      the simple malloc.
//   *  [Memory utilization] How much your malloc is memory efficient.
//      This is defined as (S1 / S2), where S1 is the total size of objects
//      allocated at the end of the challange and S2 is the total size of
//      mmap_from_system()ed regions at the end of the challenge. You can
//      improve the memory utilization by decreasing memory fragmentation and
//      reclaiming unused memory regions to the system with munmap_to_system().
// 5. This program works on Linux and Mac but not on Windows. If you don't have
//    Linux or Mac, you can use Google Cloud Shell (See
//    https://docs.google.com/document/d/1TNu8OfoQmiQKy9i2jPeGk1DOOzSVfbt4RoP_wcXgQSs/edit#).
// 6. You need to specify an '-lm' option to compile this program.
//   *  gcc malloc_challenge.c -lm
//   *  clang malloc_challenge.c -lm
//
// Enjoy! :D
//

#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/time.h>

void *mmap_from_system(size_t size);
void munmap_to_system(void *ptr, size_t size);

////////////////////////////////////////////////////////////////////////////////

//
// [Simple malloc]
//
// This is an example, straightforward implementation of malloc. Your goal is
// to invent a smarter malloc algorithm in terms of both [Execution time] and
// [Memory utilization].

// Each object or free slot has metadata just prior to it:
//
// ... | m | object | m | free slot | m | free slot | m | object | ...
//
// where |m| indicates metadata. The metadata is needed for two purposes:
//
// 1) For an allocated object:
//   *  |size| indicates the size of the object. |size| does not include
//      the size of the metadata.
//   *  |next| is unused and set to NULL.
// 2) For a free slot:
//   *  |size| indicates the size of the free slot. |size| does not include
//      the size of the metadata.
//   *  The free slots are linked with a singly linked list (we call this a
//      free list). |next| points to the next free slot.
typedef struct simple_metadata_t {
  size_t size;
  struct simple_metadata_t *next;
} simple_metadata_t;

// The global information of the simple malloc.
//   *  |free_head| points to the first free slot.
//   *  |dummy| is a dummy free slot (only used to make the free list
//      implementation simpler).
typedef struct simple_heap_t {
  simple_metadata_t *free_head;
  simple_metadata_t dummy;
} simple_heap_t;

simple_heap_t simple_heap;

// Add a free slot to the beginning of the free list.
void simple_add_to_free_list(simple_metadata_t *metadata) {
  assert(!metadata->next);
  metadata->next = simple_heap.free_head;
  simple_heap.free_head = metadata;
}

// Remove a free slot from the free list.
void simple_remove_from_free_list(simple_metadata_t *metadata,
                                  simple_metadata_t *prev) {
  if (prev) {
    prev->next = metadata->next;
  } else {
    simple_heap.free_head = metadata->next;
  }
  metadata->next = NULL;
}

// This is called only once at the beginning of each challenge.
void simple_initialize() {
  simple_heap.free_head = &simple_heap.dummy;
  simple_heap.dummy.size = 0;
  simple_heap.dummy.next = NULL;
}

// This is called every time an object is allocated. |size| is guaranteed
// to be a multiple of 8 bytes and meets 8 <= |size| <= 4000. You are not
// allowed to use any library functions other than mmap_from_system /
// munmap_to_system.
void *simple_malloc(size_t size) {
  simple_metadata_t *metadata = simple_heap.free_head;
  simple_metadata_t *prev = NULL;
  // First-fit: Find the first free slot the object fits.
  while (metadata && metadata->size < size) {
    prev = metadata;
    metadata = metadata->next;
  }

  if (!metadata) {
    // There was no free slot available. We need to request a new memory region
    // from the system by calling mmap_from_system().
    //
    //     | metadata | free slot |
    //     ^
    //     metadata
    //     <---------------------->
    //            buffer_size
    size_t buffer_size = 4096;
    simple_metadata_t *metadata =
        (simple_metadata_t *)mmap_from_system(buffer_size);
    metadata->size = buffer_size - sizeof(simple_metadata_t);
    metadata->next = NULL;
    // Add the memory region to the free list.
    simple_add_to_free_list(metadata);
    // Now, try simple_malloc() again. This should succeed.
    return simple_malloc(size);
  }

  // |ptr| is the beginning of the allocated object.
  //
  // ... | metadata | object | ...
  //     ^          ^
  //     metadata   ptr
  void *ptr = metadata + 1;
  size_t remaining_size = metadata->size - size;
  metadata->size = size;
  // Remove the free slot from the free list.
  simple_remove_from_free_list(metadata, prev);

  if (remaining_size > sizeof(simple_metadata_t)) {
    // Create a new metadata for the remaining free slot.
    //
    // ... | metadata | object | metadata | free slot | ...
    //     ^          ^        ^
    //     metadata   ptr      new_metadata
    //                 <------><---------------------->
    //                   size       remaining size
    simple_metadata_t *new_metadata = (simple_metadata_t *)((char *)ptr + size);
    new_metadata->size = remaining_size - sizeof(simple_metadata_t);
    new_metadata->next = NULL;
    // Add the remaining free slot to the free list.
    simple_add_to_free_list(new_metadata);
  }
  return ptr;
}

// This is called every time an object is freed.  You are not allowed to use
// any library functions other than mmap_from_system / munmap_to_system.
void simple_free(void *ptr) {
  // Look up the metadata. The metadata is placed just prior to the object.
  //
  // ... | metadata | object | ...
  //     ^          ^
  //     metadata   ptr
  simple_metadata_t *metadata = (simple_metadata_t *)ptr - 1;
  // Add the free slot to the free list.
  simple_add_to_free_list(metadata);
}

void simple_finalize() {}

////////////////////////////////////////////////////////////////////////////////

//
// [My malloc]
//
// Your job is to invent a smarter malloc algorithm here :)

// This is called only once at the beginning of each challenge.
void my_initialize();

// This is called every time an object is allocated. |size| is guaranteed
// to be a multiple of 8 bytes and meets 8 <= |size| <= 4000. You are not
// allowed to use any library functions other than mmap_from_system /
// munmap_to_system.
void *my_malloc(size_t size);

// This is called every time an object is freed.  You are not allowed to use
// any library functions other than mmap_from_system / munmap_to_system.
void my_free(void *ptr);

// This is called only once at the end of each challenge.
void my_finalize();

////////////////////////////////////////////////////////////////////////////////

//
// [Test]
//
// Add test cases in test(). test() is called at the beginning of the program.

void test();

////////////////////////////////////////////////////////////////////////////////
//               YOU DO NOT NEED TO READ THE CODE BELOW                       //
////////////////////////////////////////////////////////////////////////////////

// This is code to run challenges. Please do NOT modify the code.

// Vector
typedef struct object_t {
  void *ptr;
  size_t size;
  char tag; // A tag to check the object is not broken.
} object_t;

typedef struct vector_t {
  size_t size;
  size_t capacity;
  object_t *buffer;
} vector_t;

vector_t *vector_create() {
  vector_t *vector = (vector_t *)malloc(sizeof(vector_t));
  vector->capacity = 0;
  vector->size = 0;
  vector->buffer = NULL;
  return vector;
}

void vector_push(vector_t *vector, object_t object) {
  if (vector->size >= vector->capacity) {
    vector->capacity = vector->capacity * 2 + 128;
    vector->buffer = (object_t *)realloc(vector->buffer,
                                         vector->capacity * sizeof(object_t));
  }
  vector->buffer[vector->size] = object;
  vector->size++;
}

size_t vector_size(vector_t *vector) { return vector->size; }

object_t vector_at(vector_t *vector, size_t i) {
  assert(i < vector->size);
  return vector->buffer[i];
}

void vector_clear(vector_t *vector) {
  free(vector->buffer);
  vector->capacity = 0;
  vector->size = 0;
  vector->buffer = NULL;
}

void vector_destroy(vector_t *vector) {
  free(vector->buffer);
  free(vector);
}

// Return the current time in seconds.
double get_time(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec * 1e-6;
}

// Return a random number in [0, 1).
double urand() { return rand() / ((double)RAND_MAX + 1); }

// Return an object size. The returned size is a random number in
// [min_size, max_size] that follows an exponential distribution.
// |min_size| needs to be a multiple of 8 bytes.
size_t get_object_size(size_t min_size, size_t max_size) {
  const int alignment = 8;
  assert(min_size <= max_size);
  assert(min_size % alignment == 0);
  const double lambda = 1;
  const double threshold = 6;
  double tau = -lambda * log(urand());
  if (tau >= threshold) {
    tau = threshold;
  }
  size_t result = (size_t)((max_size - min_size) * tau / threshold) + min_size;
  result = result / alignment * alignment;
  assert(min_size <= result);
  assert(result <= max_size);
  return result;
}

// Return an object lifetime. The returned lifetime is a random number in
// [min_epoch, max_epoch] that follows an exponential distribution.
unsigned get_object_lifetime(unsigned min_epoch, unsigned max_epoch) {
  const double lambda = 1;
  const double threshold = 6;
  double tau = -lambda * log(urand());
  if (tau >= threshold) {
    tau = threshold;
  }
  unsigned result =
      (unsigned)((max_epoch - min_epoch) * tau / threshold + min_epoch);
  assert(min_epoch <= result);
  assert(result <= max_epoch);
  return result;
}

typedef void (*initialize_func_t)();
typedef void *(*malloc_func_t)(size_t size);
typedef void (*free_func_t)(void *ptr);
typedef void (*finalize_func_t)();

// Record the statistics of each challenge.
typedef struct stats_t {
  double begin_time;
  double end_time;
  size_t mmap_size;
  size_t munmap_size;
  size_t allocated_size;
  size_t freed_size;
} stats_t;

stats_t stats;

// Run one challenge.
// |min_size|: The min size of an allocated object
// |max_size|: The max size of an allocated object
// |*_func|: Function pointers to initialize / malloc / free.
void run_challenge(size_t min_size, size_t max_size,
                   initialize_func_t initialize_func, malloc_func_t malloc_func,
                   free_func_t free_func, finalize_func_t finalize_func) {
  const int cycles = 10;
  const int epochs_per_cycle = 100;
  const int objects_per_epoch_small = 100;
  const int objects_per_epoch_large = 2000;
  char tag = 0;
  // The last entry of the vector is used to store objects that are never freed.
  vector_t *objects[epochs_per_cycle + 1];
  for (int i = 0; i < epochs_per_cycle + 1; i++) {
    objects[i] = vector_create();
  }
  initialize_func();
  stats.mmap_size = stats.munmap_size = 0;
  stats.allocated_size = stats.freed_size = 0;
  stats.begin_time = get_time();
  for (int cycle = 0; cycle < cycles; cycle++) {
    for (int epoch = 0; epoch < epochs_per_cycle; epoch++) {
      size_t allocated = 0;
      size_t freed = 0;

      // Allocate |objects_per_epoch| objects.
      int objects_per_epoch = objects_per_epoch_small;
      if (epoch == 0) {
        // To simulate a peak memory usage, we allocate a larger number of
        // objects from time to time.
        objects_per_epoch = objects_per_epoch_large;
      }
      for (int i = 0; i < objects_per_epoch; i++) {
        size_t size = get_object_size(min_size, max_size);
        int lifetime = get_object_lifetime(1, epochs_per_cycle);
        stats.allocated_size += size;
        allocated += size;
        void *ptr = malloc_func(size);
        memset(ptr, tag, size);
        object_t object = {ptr, size, tag};
        tag++;
        if (tag == 0) {
          // Avoid 0 for tagging since it is not distinguishable from fresh
          // mmaped memory.
          tag++;
        }
        if (urand() < 0.04) {
          // 4% of objects are set as never freed.
          vector_push(objects[epochs_per_cycle], object);
        } else {
          vector_push(objects[(epoch + lifetime) % epochs_per_cycle], object);
        }
      }

      // Free objects that are expected to be freed in this epoch.
      vector_t *vector = objects[epoch];
      for (size_t i = 0; i < vector_size(vector); i++) {
        object_t object = vector_at(vector, i);
        stats.freed_size += object.size;
        freed += object.size;
        // Check that the tag is not broken.
        if (((char *)object.ptr)[0] != object.tag ||
            ((char *)object.ptr)[object.size - 1] != object.tag) {
          printf("An allocated object is broken!");
          assert(0);
        }
        free_func(object.ptr);
      }

#if 0
      // Debug print
      printf("epoch = %d, allocated = %ld bytes, freed = %ld bytes\n",
             cycle * epochs_per_cycle + epoch, allocated, freed);
      printf("allocated = %.2f MB, freed = %.2f MB, mmap = %.2f MB, munmap = %.2f MB, utilization = %d%%\n",
             stats.allocated_size / 1024.0 / 1024.0,
             stats.freed_size / 1024.0 / 1024.0,
             stats.mmap_size / 1024.0 / 1024.0,
             stats.munmap_size / 1024.0 / 1024.0,
             (int)(100.0 * (stats.allocated_size - stats.freed_size)
                   / (stats.mmap_size - stats.munmap_size)));
#endif
      vector_clear(vector);
    }
  }
  stats.end_time = get_time();
  for (int i = 0; i < epochs_per_cycle + 1; i++) {
    vector_destroy(objects[i]);
  }
  finalize_func();
}

// Print stats
void print_stats(char *challenge, stats_t simple_stats, stats_t my_stats) {
  printf("%s: simple malloc => my malloc\n", challenge);
  printf("Time: %.f ms => %.f ms\n",
         (simple_stats.end_time - simple_stats.begin_time) * 1000,
         (my_stats.end_time - my_stats.begin_time) * 1000);
  printf("Utilization: %d%% => %d%%\n",
         (int)(100.0 * (simple_stats.allocated_size - simple_stats.freed_size) /
               (simple_stats.mmap_size - simple_stats.munmap_size)),
         (int)(100.0 * (my_stats.allocated_size - my_stats.freed_size) /
               (my_stats.mmap_size - my_stats.munmap_size)));
  printf("==================================\n");
}

// Run challenges
void run_challenges() {
  stats_t simple_stats, my_stats;

  // Warm up run.
  run_challenge(128, 128, simple_initialize, simple_malloc, simple_free,
                simple_finalize);

  // Challenge 1:
  run_challenge(128, 128, simple_initialize, simple_malloc, simple_free,
                simple_finalize);
  simple_stats = stats;
  run_challenge(128, 128, my_initialize, my_malloc, my_free, my_finalize);
  my_stats = stats;
  print_stats("Challenge 1", simple_stats, my_stats);

  // Challenge 2:
  run_challenge(16, 16, simple_initialize, simple_malloc, simple_free,
                simple_finalize);
  simple_stats = stats;
  run_challenge(16, 16, my_initialize, my_malloc, my_free, my_finalize);
  my_stats = stats;
  print_stats("Challenge 2", simple_stats, my_stats);

  // Challenge 3:
  run_challenge(16, 128, simple_initialize, simple_malloc, simple_free,
                simple_finalize);
  simple_stats = stats;
  run_challenge(16, 128, my_initialize, my_malloc, my_free, my_finalize);
  my_stats = stats;
  print_stats("Challenge 3", simple_stats, my_stats);

  // Challenge 4:
  run_challenge(256, 4000, simple_initialize, simple_malloc, simple_free,
                simple_finalize);
  simple_stats = stats;
  run_challenge(256, 4000, my_initialize, my_malloc, my_free, my_finalize);
  my_stats = stats;
  print_stats("Challenge 4", simple_stats, my_stats);

  // Challenge 5:
  run_challenge(8, 4000, simple_initialize, simple_malloc, simple_free,
                simple_finalize);
  simple_stats = stats;
  run_challenge(8, 4000, my_initialize, my_malloc, my_free, my_finalize);
  my_stats = stats;
  print_stats("Challenge 5", simple_stats, my_stats);
}

// Allocate a memory region from the system. |size| needs to be a multiple of
// 4096 bytes.
void *mmap_from_system(size_t size) {
  assert(size % 4096 == 0);
  stats.mmap_size += size;
  void *ptr = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  assert(ptr);
  return ptr;
}

// Free a memory region [ptr, ptr + size) to the system. |ptr| and |size| needs
// to be a multiple of 4096 bytes.
void munmap_to_system(void *ptr, size_t size) {
  assert(size % 4096 == 0);
  assert((uintptr_t)(ptr) % 4096 == 0);
  stats.munmap_size += size;
  int ret = munmap(ptr, size);
  assert(ret != -1);
}

int main(int argc, char **argv) {
  srand(12); // Set the rand seed to make the challenges non-deterministic.
  test();
  run_challenges();
  return 0;
}
