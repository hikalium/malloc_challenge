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
  char s[2 + (16 + 1) * 2 + 10];
  char* wc = &s[0];
  write_string(&wc, "a ");
  write_uint64_hex(&wc, (uint64_t)p);
  write_string(&wc, " ");
  write_uint64_hex(&wc, size);
  write_string(&wc, "\n");
  write(trace_fd, s, wc - s);
}

void trace_print_free(void* p) {
  char s[2 + 16 + 1 + 10];
  char* wc = &s[0];
  write_string(&wc, "f ");
  write_uint64_hex(&wc, (uint64_t)p);
  write_string(&wc, "\n");
  write(trace_fd, s, wc - s);
}

void trace_print_realloc(void* new_p, size_t size, void* old_p) {
  char s[2 + (16 + 1) * 3 + 10];
  char* wc = &s[0];
  write_string(&wc, "r ");
  write_uint64_hex(&wc, (uint64_t)new_p);
  write_string(&wc, " ");
  write_uint64_hex(&wc, size);
  write_string(&wc, " ");
  write_uint64_hex(&wc, (uint64_t)old_p);
  write_string(&wc, "\n");
  write(trace_fd, s, wc - s);
}

static void init_trace_fp() {
  if (trace_fd) {
    return;
  }
  char s[64];
  char* wc = &s[0];
  write_string(&wc, "trace_");
  write_uint64_hex(&wc, (uint64_t)&trace_fd);
  write_string(&wc, ".txt");
  trace_fd = creat(s, 0644);
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

#define TMP_BUFFER_SIZE 4096
static char tmp_buffer[TMP_BUFFER_SIZE];
static int tmp_buffer_used;
void* calloc(size_t n, size_t elem_size) {
  static void* (*original_calloc)(size_t, size_t);
  static int initializing;
  if (!original_calloc) {
    init_trace_fp();
    if (initializing) {
      if (tmp_buffer_used + n * elem_size >= TMP_BUFFER_SIZE) {
        fprintf(stderr, "No more tmp_buffer\n");
        exit(EXIT_FAILURE);
      }
      void* p = &tmp_buffer[tmp_buffer_used];
      tmp_buffer_used += n * elem_size;
      trace_print_malloc(p, elem_size * n);
      return p;
    }
    original_calloc = dlsym(RTLD_NEXT, "calloc");
  }
  void* p = original_calloc(n, elem_size);
  trace_print_malloc(p, elem_size * n);
  return p;
}

void free(void* p) {
  if (!p) return;
  static void (*original_free)(void*);
  if (!original_free) {
    init_trace_fp();
    original_free = dlsym(RTLD_NEXT, "free");
  }
  if ((uint64_t)tmp_buffer <= (uint64_t)p &&
      (uint64_t)p < (uint64_t)tmp_buffer + TMP_BUFFER_SIZE) {
    // skip
  } else {
    original_free(p);
  }
  trace_print_free(p);
}

void* realloc(void* p, size_t size) {
  static void* (*original_realloc)(void*, size_t);
  if (!original_realloc) {
    init_trace_fp();
    original_realloc = dlsym(RTLD_NEXT, "realloc");
  }
  void* new_p = original_realloc(p, size);
  trace_print_realloc(new_p, size, p);
  return new_p;
}

void* reallocarray(void* p, size_t n, size_t elem_size) {
  fprintf(stderr, "reallocarray called\n");
  exit(EXIT_FAILURE);
}
