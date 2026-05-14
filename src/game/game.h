
#pragma once
#include "../engine/ecs/archetype.h"
#include "../engine/ecs/world.h"
#include "../engine/util/bitset.h"
#include "components/components.h"
#include "ecs_get.h"
#include "raylib.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define NUM_ARCHETYPES 1024
#define TOTAL_ENTITIES 1000000

typedef struct {
  world_t *world;
  componentPool_t timerPool;
} Engine;

typedef struct GameWorld {
  uint32_t archIds[NUM_ARCHETYPES];
  uint32_t archCount;
} GameWorld;

Engine EngineInit(void);
void EngineShutdown(Engine *engine);

GameWorld GameWorldCreate(Engine *engine, world_t *world);
