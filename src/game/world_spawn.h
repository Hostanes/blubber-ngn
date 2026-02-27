
#include "ecs_get.h"
#include "game.h"
#include "level_creater_helper.h"
#include "systems/systems.h"
#include <raylib.h>
#include <raymath.h>

GameWorld GameWorldCreate(Engine *engine, world_t *world);
void SpawnLevel01(world_t *world, GameWorld *gw);
