
#include "../game.h"
#include "systems.h"
#include <stdint.h>

void BulletSystem(world_t *world, archetype_t *bulletArch,
                  archetype_t *enemyArch, float dt) {
  for (uint32_t i = 0; i < bulletArch->count; i++) {
    entity_t b = bulletArch->entities[i];

    Active *active = ECS_GET(world, b, Active, COMP_ACTIVE);
    if (!active->value)
      continue;

    Timer *life = ECS_GET(world, b, Timer, COMP_TIMER);
    if (life->value <= 0.0f) {
      active->value = false;
      continue;
    }

    Position *pos = ECS_GET(world, b, Position, COMP_POSITION);
    Velocity *vel = ECS_GET(world, b, Velocity, COMP_VELOCITY);

    Vector3 prevPos = pos->value;
    Vector3 nextPos = Vector3Add(prevPos, Vector3Scale(vel->value, dt));
    Vector3 delta = Vector3Subtract(nextPos, prevPos);

    float length = Vector3Length(delta);

    if (length > 0.0001f) {
      Vector3 dir = Vector3Scale(delta, 1.0f / length);

      Ray ray = {prevPos, dir};

      // Check against enemies
      for (uint32_t j = 0; j < enemyArch->count; j++) {
        entity_t enemy = enemyArch->entities[j];

        Active *enemyActive = ECS_GET(world, enemy, Active, COMP_ACTIVE);

        if (!enemyActive->value)
          continue;

        CollisionInstance *ci =
            ECS_GET(world, enemy, CollisionInstance, COMP_COLLISION_INSTANCE);

        if (!ci)
          continue;

        RayCollision hit = GetRayCollisionBox(ray, ci->worldBounds);

        if (hit.hit && hit.distance <= length) {
          // Apply damage here
          // DamageEnemy(enemy);
          printf("HIT\n");
          active->value = false;
          break;
        }
      }
    }

    // Move bullet after sweep
    pos->value = nextPos;

    life->value -= dt;
  }
}
