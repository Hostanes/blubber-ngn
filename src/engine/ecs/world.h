#pragma once
#include "../util/bitset.h"
#include "archetype.h"
#include "ecs_types.h"
#include "entity.h"

typedef struct world_t world_t;

world_t *WorldCreate(void);
void WorldDestroy(world_t *);

entity_t WorldCreateEntity(world_t *, const bitset_t *mask);
void WorldDestroyEntity(world_t *, entity_t);

void *WorldGetComponent(world_t *, entity_t, componentId_t);

archetype_t *WorldCreateArchetype(world_t *world, const bitset_t *mask);
