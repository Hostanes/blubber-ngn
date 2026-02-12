#pragma once
#include "../engine/util/bitset.h"

#define ECS_GET(world, entity, Type, ID)                                       \
  ((Type *)WorldGetComponent(world, entity, ID))

#define OMP_MIN_ITERATIONS 1024

static bitset_t MakeMask(uint32_t *bits, uint32_t count) {
  bitset_t mask;
  BitsetInit(&mask, 64);
  for (uint32_t i = 0; i < count; ++i)
    BitsetSet(&mask, bits[i]);
  return mask;
}
