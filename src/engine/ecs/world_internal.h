
#pragma once
#include "../util/bitset.h"
#include "component_internal.h"
#include "entity_internal.h"

#define MaxComponents 64

struct world_t {
  entityManager_t entityManager;

  componentPool_t components[MaxComponents];
  uint32_t componentCount;

  Bitset *entityMasks;
};
