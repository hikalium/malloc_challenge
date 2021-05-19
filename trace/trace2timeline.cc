#include <iostream>
#include <unordered_map>

std::unordered_map<uint64_t, uint64_t> alloc_sizes;
uint64_t peak_size = 0;
uint64_t resident_size = 0;
uint64_t allocation_size_accumlated = 0;

void record_alloc(uint64_t addr, uint64_t size) {
  alloc_sizes.insert({addr, size});
  resident_size += size;
  allocation_size_accumlated += size;
  peak_size = std::max(peak_size, resident_size);
}

void record_free(uint64_t addr) {
  const auto &it = alloc_sizes.find(addr);
  if (it == alloc_sizes.end()) {
    printf("Addr 0x%lX is being freed but not allocated\n", addr);
    return;
  }
  alloc_sizes.erase(it);
  resident_size -= (*it).second;
}

int main() {
  char op;
  uint64_t addr;
  uint64_t count = 0;
  while (scanf(" %c %lX", &op, &addr) == 2) {
    if (op == 'a') {
      uint64_t size;
      if (scanf(" %lX", &size) != 1) {
        printf("Failed to read size for alloc");
        exit(EXIT_FAILURE);
      }
      record_alloc(addr, size);
      count++;
      continue;
    }
    if (op == 'r') {
      uint64_t size, old_addr;
      if (scanf(" %lX %lX", &size, &old_addr) != 2) {
        printf("Failed to read size and old_addr for realloc");
        exit(EXIT_FAILURE);
      }
      // free
      if (old_addr) {
        record_free(old_addr);
      }
      record_alloc(addr, size);
      count++;
      continue;
    }
    if (op == 'f') {
      record_free(addr);
      count++;
      continue;
    }
    printf("Unknown op: %c at count %ld\n", op, count);
    exit(EXIT_FAILURE);
  }
  printf("count: %ld\n", count);
  printf("peak_size: %ld\n", peak_size);
  printf("resident_size at last: %ld\n", resident_size);
  printf("allocation_size_accumlated: %ld\n", allocation_size_accumlated);
  return 0;
}
