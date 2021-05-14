#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

void* malloc(size_t size) {
  static void* (*original_malloc)(size_t);
  if (!original_malloc) {
    original_malloc = dlsym(RTLD_NEXT, "malloc");
  }
  void *p = original_malloc(size);
  fprintf(stderr, "a %p %lX\n", p, size);
  return p;
}

void free(void* p) {
  if(!p) return;
  static void (*original_free)(void*);
  if (!original_free) {
    original_free = dlsym(RTLD_NEXT, "free");
  }
  original_free(p);
  fprintf(stderr, "f %p\n", p);
}
