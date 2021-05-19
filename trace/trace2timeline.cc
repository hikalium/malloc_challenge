#include <iostream>
#include <unordered_map>

std::unordered_map<int64_t, int64_t> alloc_sizes;
int64_t peak_size = 0;
int64_t resident_size = 0;
int64_t allocation_size_accumlated = 0;
int64_t free_size_accumlated = 0;

void record_alloc(int64_t addr, int64_t size) {
  alloc_sizes.insert({addr, size});
  resident_size += size;
  allocation_size_accumlated += size;
  peak_size = std::max(peak_size, resident_size);
}

void record_free(int64_t addr) {
  const auto &it = alloc_sizes.find(addr);
  if (it == alloc_sizes.end()) {
    printf("Addr 0x%lX is being freed but not allocated\n", addr);
    return;
  }
  resident_size -= (*it).second;
  free_size_accumlated += (*it).second;
  alloc_sizes.erase(it);
}

int main() {
  char op;
  int64_t addr;
  int64_t count = 0;
  int64_t last_resident_size = 0;
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
    printf("%ld\t%ld\t%ld\t%ld\t%ld\n", count, resident_size, allocation_size_accumlated, resident_size - last_resident_size, free_size_accumlated);
    last_resident_size = resident_size;
    count++;
  }
  fprintf(stderr, "count: %ld\n", count);
  fprintf(stderr, "peak_size: %ld\n", peak_size);
  fprintf(stderr, "resident_size at last: %ld\n", resident_size);
  fprintf(stderr, "allocation_size_accumlated: %ld\n",
          allocation_size_accumlated);
  return 0;
}
