
#include "../../engine/ecs/world.h"
#include "../components/components.h"
#include "../components/movement.h"
#include "../components/transform.h"
#include "../ecs_get.h"
#include "raylib.h"
#include "raymath.h"
#include <stdint.h>

void PlayerControlSystem(world_t *world, entity_t player);
void PlayerWeaponSystem(world_t *world, entity_t player);
void MovementSystem(world_t *world, archetype_t *arch, float dt);
void RenderSystem(world_t *world, archetype_t *arch);
void TimerSystem(componentPool_t *timerPool, float dt);
