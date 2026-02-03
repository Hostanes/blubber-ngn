
#pragma once
#include "ecs_types.h"

typedef struct world_t world_t;

world_t *WorldCreate(void);
void WorldDestroy(world_t *);

entity_t WorldCreateEntity(world_t *);
void WorldDestroyEntity(world_t *, entity_t);

void *WorldAddComponent(world_t *, entity_t, componentId_t);

void WorldRemoveComponent(world_t *, entity_t, componentId_t);

void *WorldGetComponent(world_t *, entity_t, componentId_t);
