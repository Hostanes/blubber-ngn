#pragma once
#include "ecs_types.h"
#include <stddef.h>
#include <stdint.h>

struct componentPool_t {
  entity_t *denseEntities;
  void *denseData;

  uint32_t *sparse;
  uint32_t count;
  uint32_t denseCapacity;
  uint32_t sparseCapacity;

  size_t elementSize;
};
