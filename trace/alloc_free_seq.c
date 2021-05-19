#include <stdlib.h>

#define MALLOC_COUNT 1000
#define MALLOC_SIZE 128

void *allocated[MALLOC_COUNT];

int main() {
  for(int i = 0; i < MALLOC_COUNT; i++){
    allocated[i] = malloc(MALLOC_SIZE);
  }
  for(int i = 0; i < MALLOC_COUNT; i++){
    free(allocated[i]);
  }
  return 0;
}
