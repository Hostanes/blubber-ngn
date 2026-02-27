
#pragma once
#include "../../engine/ecs/world.h"
#include "../../engine/math/aabb.h"

typedef void (*OnDeathFn)(world_t *world, entity_t e);

typedef struct {
  OnDeathFn fn;
} OnDeath;

typedef void (*OnCollisionFn)(world_t *world, entity_t self, entity_t other);

typedef struct {
  OnCollisionFn fn;
} OnCollision;
