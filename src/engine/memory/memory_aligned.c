
#define _POSIX_C_SOURCE 200809L
#include "memory_aligned.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// rounds up size to next multiple of ECS_ALIGNMENT
// either 32 or 64 work well, we use 64 for now
static size_t AlignSize(size_t size) {
  size_t alignment = ECS_ALIGNMENT;
  return (size + alignment - 1) & ~(alignment - 1);
}

void *ECS_AlignedAlloc(size_t size) {
  size = AlignSize(size);

#if defined(_MSC_VER) // for windows
  void *ptr = _aligned_malloc(size, ECS_ALIGNMENT);
  if (!ptr) {
    fprintf(stderr, "Aligned allocation failed\n");
    exit(1);
  }
  return ptr;
#else // for linux
  void *ptr = NULL;
  if (posix_memalign(&ptr, ECS_ALIGNMENT, size) != 0) {
    fprintf(stderr, "Aligned allocation failed\n");
    exit(1);
  }
  return ptr;
#endif
}

void ECS_AlignedFree(void *ptr) {
#if defined(_MSC_VER)
  _aligned_free(ptr);
#else
  free(ptr);
#endif
}

void *ECS_AlignedRealloc(void *oldPtr, size_t oldSize, size_t newSize) {
  void *newPtr = ECS_AlignedAlloc(newSize);

  if (oldPtr) {
    memcpy(newPtr, oldPtr, oldSize);
    ECS_AlignedFree(oldPtr);
  }

  return newPtr;
}
