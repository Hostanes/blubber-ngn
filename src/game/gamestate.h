#pragma once
#include "../engine/ecs/archetype.h"
#include "../engine/ecs/component.h"
#include "../engine/ecs/entity.h"
#include "../engine/ecs/entity_internal.h"
#include "../engine/ecs/world.h"
#include "../engine/util/bitset.h"
#include "gametypes.h"
#include "raylib.h"
#include "raymath.h"

typedef struct {
  world_t *world;

  componentPool_t modelCollectionPool;

} GameState_t;

void GameStateInit(GameState_t *gs);

static bitset_t MakeMask(uint32_t *bits, uint32_t count) {
  bitset_t mask;
  BitsetInit(&mask, 64);
  for (uint32_t i = 0; i < count; i++)
    BitsetSet(&mask, bits[i]);
  return mask;
}
