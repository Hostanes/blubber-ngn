#include "../game.h"
#include "systems.h"
#include <stdint.h>

static bitset_t gravityMask;
static bool gravityMaskInit = false;

static void EnsureGravityMask(void) {
  if (gravityMaskInit)
    return;

  BitsetInit(&gravityMask, 64);
  BitsetSet(&gravityMask, COMP_GRAVITY);

  gravityMaskInit = true;
}

void ApplyGravity(world_t *world, GameWorld *game, float dt) {
  EnsureGravityMask();

  for (uint32_t i = 0; i < world->archetypeCount; ++i) {
    archetype_t *arch = &world->archetypes[i];

    if (!BitsetContainsAll(&arch->mask, &gravityMask))
      continue;

    for (uint32_t e = 0; e < arch->count; e++) {
      entity_t entity = arch->entities[e];

      Position *pos = ECS_GET(world, entity, Position, COMP_POSITION);
      Velocity *vel = ECS_GET(world, entity, Velocity, COMP_VELOCITY);

      vel->value.y -= 2.5 * 10.0f * dt;

      float halfHeight = 2.0f; // capsule half height
      float groundY = HeightMap_GetHeightSmooth(&game->terrainHeightMap,
                                                pos->value.x, pos->value.z);

      float footY = pos->value.y - halfHeight;

      if (footY < groundY) {
        pos->value.y = groundY + halfHeight;

        if (vel->value.y < 0.0f) {
          vel->value.y = 0.0f;
        }
      }
    }
  }
}
