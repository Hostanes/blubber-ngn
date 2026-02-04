
#pragma once
#include "../util/bitset.h"
#include "component.h"
#include "ecs_types.h"
#include <stddef.h>
#include <stdint.h>

typedef enum {
  ArchetypeStorageInline,
  ArchetypeStoragePool
} archetypeStorageType_t;

typedef struct {
  componentId_t componentId;
  archetypeStorageType_t storageType;

  size_t elementSize;

  void *data;            // inline storage (SoA)
  componentPool_t *pool; // pooled storage
} archetypeColumn_t;

struct archetype_t {
  bitset_t mask;

  entity_t *entities;
  uint32_t count;
  uint32_t capacity;

  archetypeColumn_t *columns;
  uint32_t columnCount;
};
