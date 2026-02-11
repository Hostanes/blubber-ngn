#pragma once
#include "../../engine/ecs/world.h"
#include "../../engine/math/heightmap.h"
#include "../components/components.h"
#include "../components/movement.h"
#include "../components/transform.h"
#include "../ecs_get.h"
#include "raylib.h"
#include "raymath.h"
#include <stdint.h>

typedef struct GameWorld GameWorld;

void PlayerControlSystem(world_t *world, GameWorld *game, entity_t player,
                         float dt);
void PlayerWeaponSystem(world_t *world, entity_t player, float dt);
void MovementSystem(world_t *world, archetype_t *arch, float dt);
void RenderLevelSystem(world_t *world, GameWorld *game, Camera *camera);
void RenderMainMenu(GameWorld *game);
void TimerSystem(componentPool_t *timerPool, float dt);
void ApplyGravity(world_t *world, GameWorld *game, float dt);
