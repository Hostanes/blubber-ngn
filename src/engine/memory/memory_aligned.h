#pragma once
#include <stddef.h>

#define ECS_ALIGNMENT 64

#if defined(_MSC_VER)
#define ECS_ALIGN(x) __declspec(align(x))
#else
#define ECS_ALIGN(x) __attribute__((aligned(x)))
#endif

void *ECS_AlignedAlloc(size_t size);
void ECS_AlignedFree(void *ptr);
void *ECS_AlignedRealloc(void *oldPtr, size_t oldSize, size_t newSize);
