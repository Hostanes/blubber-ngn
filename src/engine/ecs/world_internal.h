#pragma once
#include "../util/bitset.h"
#include "archetype_internal.h"
#include "component_internal.h"
#include "entity_internal.h"
#include <stdint.h>

#define maxArchetypes 128
#define maxComponents 64

typedef struct {
  archetype_t *archetype;
  uint32_t index; // index inside archetype
} entityLocation_t;

struct world_t {
  entityManager_t entityManager;

  archetype_t *archetypes;
  uint32_t archetypeCount;
  uint32_t archetypeCapacity;

  entityLocation_t *entityLocations;
  uint32_t entityLocationCapacity;
};
