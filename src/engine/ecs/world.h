#pragma once
#include "../util/bitset.h"
#include "archetype.h"
#include "ecs_types.h"
#include "entity.h"
#include "world_internal.h"
#include <stdint.h>

#define ECS_GET(world, entity, Type, ID)                                       \
  ((Type *)WorldGetComponent(world, entity, ID))

typedef struct world_t world_t;

world_t *WorldCreate(void);
void WorldDestroy(world_t *);

entity_t WorldCreateEntity(world_t *, const bitset_t *mask);
void WorldDestroyEntity(world_t *, entity_t);

void *WorldGetComponent(world_t *, entity_t, componentId_t);

uint32_t WorldCreateArchetype(world_t *world, const bitset_t *mask);

static inline archetype_t *WorldGetArchetype(world_t *world, uint32_t id) {
  return &world->archetypes[id];
}
