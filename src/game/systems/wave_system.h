#pragma once
#include "../../engine/ecs/world.h"

typedef struct GameWorld GameWorld;

void WaveSystem_Init(GameWorld *gw);
void WaveSystem_Update(world_t *world, GameWorld *gw, float dt);
