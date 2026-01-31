

#include "ecs_types.h"
#include <cstdint>
#include <stddef.h>

typedef struct {
  entity_t *denseEntities;
  void *denseData;

  uint32_t *sparse;
  uint32_t count;
  uint32_t capacity;

  size_t elementSize;
} ComponentPool_t;
