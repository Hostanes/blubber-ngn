#include "../game.h"
#include "systems.h"

void EnemyAISystem(world_t *world, GameWorld *game, archetype_t *enemyArch,
                   float dt) {
  for (uint32_t i = 0; i < enemyArch->count; i++) {
    entity_t e = enemyArch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value)
      continue;

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    Velocity *vel = ECS_GET(world, e, Velocity, COMP_VELOCITY);
    NavPath *path = ECS_GET(world, e, NavPath, COMP_NAVPATH);

    if (!path || path->count == 0)
      continue;

    if (path->currentIndex >= path->count) {
      vel->value.x = 0;
      vel->value.z = 0;
      continue;
    }
    pos->value.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                                 pos->value.x, pos->value.z);

    Vector3 target = path->points[path->currentIndex];

    Vector3 toTarget = Vector3Subtract(target, pos->value);
    toTarget.y = 0.0f;

    float distance = Vector3Length(toTarget);
    // printf("target = %f, %f\n:", toTarget.x, toTarget.z);
    const float arriveThreshold = 0.5f;
    const float moveSpeed = 15.0f;

    // If close to waypoint -> advance
    if (distance < arriveThreshold) {
      path->currentIndex++;

      // If that was the final node -> STOP completely
      if (path->currentIndex >= path->count) {
        vel->value.x = 0;
        vel->value.z = 0;
        path->count = 0;        // clear path
        path->currentIndex = 0; // reset

        continue;
      }

      continue;
    }

    // Move toward waypoint
    if (distance > 0.001f) {
      Vector3 dir = Vector3Normalize(toTarget);

      vel->value.x = dir.x * moveSpeed;
      vel->value.z = dir.z * moveSpeed;
    }
  }
}
