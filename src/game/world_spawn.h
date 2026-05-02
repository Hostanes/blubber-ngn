
#include "ecs_get.h"
#include "game.h"
#include "level_creater_helper.h"
#include "systems/systems.h"
#include <raylib.h>
#include <raymath.h>

GameWorld GameWorldCreate(Engine *engine, world_t *world);
void RegisterAllArchetypes(Engine *engine, GameWorld *gw, world_t *world);
void SpawnLevelFromFile(world_t *world, GameWorld *gw, const char *path);
void SpawnLevel01(world_t *world, GameWorld *gw);
void SpawnLevel02(world_t *world, GameWorld *gw);
void SpawnLevel03(world_t *world, GameWorld *gw);
void SpawnLevel04(world_t *world, GameWorld *gw);
