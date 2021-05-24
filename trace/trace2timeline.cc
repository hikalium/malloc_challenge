#include <stdio.h>

#include <iostream>
#include <limits>
#include <unordered_map>

std::unordered_map<int64_t, int64_t> alloc_sizes;
int64_t peak_size = 0;
int64_t resident_size = 0;
int64_t allocation_size_accumlated = 0;
int64_t free_size_accumlated = 0;
FILE *trace_fp;
int64_t range_begin = std::numeric_limits<int64_t>::max();
int64_t range_end = std::numeric_limits<int64_t>::min();

/*
output trace format:
a <begin_addr> <end_addr>
f <begin_addr> <end_addr>
r <begin_addr> <end_addr>
*/
void trace_op(char op, int64_t addr, int64_t size) {
  // Trace addr < 0x1'0000'0000LL ops only to ease visualization
  fprintf(trace_fp, "%c %ld %ld\n", op, addr, size);
  range_begin = std::min(range_begin, addr);
  range_end = std::max(range_end, addr + size);
}

void record_alloc(int64_t addr, int64_t size) {
  alloc_sizes.insert({addr, size});
  resident_size += size;
  allocation_size_accumlated += size;
  peak_size = std::max(peak_size, resident_size);
  trace_op('a', addr, size);
}


void record_free(int64_t addr) {
  const auto &it = alloc_sizes.find(addr);
  if (it == alloc_sizes.end()) {
    printf("Addr 0x%lX is being freed but not allocated\n", addr);
    return;
  }
  const int64_t size = (*it).second;
  alloc_sizes.erase(it);

  resident_size -= size;
  free_size_accumlated += size;
  trace_op('f', addr, size);
}

int main() {
  char op;
  int64_t addr;
  int64_t count = 0;
  int64_t last_resident_size = 0;
  trace_fp = fopen("trace.txt", "wb");
  if (!trace_fp) {
    printf("Failed to open trace file");
    exit(EXIT_FAILURE);
  }
  while (scanf(" %c %lX", &op, (uint64_t *)&addr) == 2) {
    if (op == 'a') {
      int64_t size;
      if (scanf(" %lX", (uint64_t *)&size) != 1) {
        printf("Failed to read size for alloc");
        exit(EXIT_FAILURE);
      }
      record_alloc(addr, size);
    } else if (op == 'r') {
      int64_t size, old_addr;
      if (scanf(" %lX %lX", (uint64_t *)&size, (uint64_t *)&old_addr) != 2) {
        printf("Failed to read size and old_addr for realloc");
        exit(EXIT_FAILURE);
      }
      // free
      if (old_addr) {
        record_free(old_addr);
      }
      record_alloc(addr, size);
    } else if (op == 'f') {
      record_free(addr);
    } else {
      printf("Unknown op: %c at count %ld\n", op, count);
      exit(EXIT_FAILURE);
    }
    printf("%ld\t%ld\t%ld\t%ld\t%ld\n", count, resident_size,
           allocation_size_accumlated, resident_size - last_resident_size,
           free_size_accumlated);
    last_resident_size = resident_size;
    count++;
  }
  fprintf(stderr, "count: %ld\n", count);
  fprintf(stderr, "peak_size: %ld\n", peak_size);
  fprintf(stderr, "resident_size at last: %ld\n", resident_size);
  fprintf(stderr, "allocation_size_accumlated: %ld\n",
      allocation_size_accumlated);
  fprintf(stderr, "range_begin: %ld\n",
          range_begin);
  fprintf(stderr, "range_end: %ld\n",
          range_end);
  fprintf(stderr, "range_size: %ld\n",
          range_end - range_begin);
  return 0;
}
