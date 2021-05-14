#include <dlfcn.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static int trace_fd;

void write_uint64_hex(char** wc, uint64_t value) {
  int i;
  char c;
  for (i = 15; i > 0; i--) {
    if ((value >> (4 * i)) & 0xF) break;
  }
  for (; i >= 0; i--) {
    c = (value >> (4 * i)) & 0xF;
    if (c < 10)
      c += '0';
    else
      c += 'A' - 10;
    **wc = c;
    (*wc)++;
  }
  **wc = 0;
}

void write_string(char** wc, char* s) {
  while (*s) {
    **wc = *s;
    (*wc)++;
    s++;
  }
  **wc = 0;
}

void trace_print_malloc(void* p, size_t size) {
  char s[2 + 16 + 1 + 16 + 1 + 1];
  char *wc = &s[0];
  write_string(&wc, "a ");
  write_uint64_hex(&wc, (uint64_t)p);
  write_string(&wc, " ");
  write_uint64_hex(&wc, size);
  write_string(&wc, "\n");
  write(trace_fd, s, wc - s);
}

void trace_print_free(void* p) {
  char s[2 + 16 + 1 + 1];
  char *wc = &s[0];
  write_string(&wc, "f ");
  write_uint64_hex(&wc, (uint64_t)p);
  write_string(&wc, "\n");
  write(trace_fd, s, wc - s);
}

static void init_trace_fp() {
  if (trace_fd) {
    return;
  }
  trace_fd = creat("trace.txt", 0644);
  if (trace_fd == -1) {
    fprintf(stderr, "init_trace_fp() failed.\n");
    exit(EXIT_FAILURE);
  }
}

void* malloc(size_t size) {
  static void* (*original_malloc)(size_t);
  if (!original_malloc) {
    init_trace_fp();
    original_malloc = dlsym(RTLD_NEXT, "malloc");
  }
  void* p = original_malloc(size);
  trace_print_malloc(p, size);
  return p;
}

void free(void* p) {
  if (!p) return;
  static void (*original_free)(void*);
  if (!original_free) {
    init_trace_fp();
    original_free = dlsym(RTLD_NEXT, "free");
  }
  original_free(p);
  trace_print_free(p);
}
