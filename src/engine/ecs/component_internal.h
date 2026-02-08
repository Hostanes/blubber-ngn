#pragma once
#include "ecs_types.h"
#include <stddef.h>
#include <stdint.h>

struct componentPool_t {
  uint32_t *denseHandles;
  void *denseData;

  uint32_t *sparse;
  uint32_t count;
  uint32_t denseCapacity;
  uint32_t sparseCapacity;

  uint32_t nextHandle;
  size_t elementSize;
};
