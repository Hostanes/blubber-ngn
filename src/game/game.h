
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

typedef struct {
  world_t *world;
} Engine;

typedef struct GameWorld {
  uint32_t benchArchId;
} GameWorld;

Engine EngineInit(void);
void EngineShutdown(Engine *engine);

GameWorld GameWorldCreate(Engine *engine, world_t *world);
