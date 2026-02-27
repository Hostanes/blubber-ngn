
#pragma once
#include "../util/bitset.h"
#include "component.h"
#include "ecs_types.h"
#include <stddef.h>
#include <stdint.h>

typedef enum {
  ArchetypeStorageInline,
  ArchetypeStorageHandle
} archetypeStorageType_t;

typedef struct {
  componentId_t componentId;
  archetypeStorageType_t storageType;

  size_t elementSize;

  void *data; // SoA array:
              // - inline value (Position, Velocity)
              // - OR handle (uint32_t, index, pointer, etc)

  // knows where handles point, only used in case of handles
  componentPool_t *pool;

  // currently unused
  void *externalStore; // optional:
                       // - TimerPool*
                       // - ModelStore*
} archetypeColumn_t;

struct archetype_t {
  uint32_t id; // id also used as index in gamestate
  bitset_t mask;

  entity_t *entities;
  uint32_t count;
  uint32_t capacity;

  archetypeColumn_t *columns;
  uint32_t columnCount;
};
