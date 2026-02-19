
#include "../game.h"
#include "systems.h"
#include <stdint.h>

static bool SweptSphereVsAABB(Vector3 start, Vector3 end, float radius,
                              BoundingBox box) {
  Vector3 velocity = Vector3Subtract(end, start);

  // Expand box by sphere radius
  BoundingBox expanded;
  expanded.min = Vector3Subtract(box.min, (Vector3){radius, radius, radius});
  expanded.max = Vector3Add(box.max, (Vector3){radius, radius, radius});

  float tMin = 0.0f;
  float tMax = 1.0f;

  for (int axis = 0; axis < 3; axis++) {
    float startVal = ((float *)&start)[axis];
    float velVal = ((float *)&velocity)[axis];
    float minVal = ((float *)&expanded.min)[axis];
    float maxVal = ((float *)&expanded.max)[axis];

    if (fabsf(velVal) < 0.000001f) {
      // Not moving along this axis
      if (startVal < minVal || startVal > maxVal)
        return false;
    } else {
      float invVel = 1.0f / velVal;
      float t1 = (minVal - startVal) * invVel;
      float t2 = (maxVal - startVal) * invVel;

      if (t1 > t2) {
        float tmp = t1;
        t1 = t2;
        t2 = tmp;
      }

      if (t1 > tMin)
        tMin = t1;
      if (t2 < tMax)
        tMax = t2;

      if (tMin > tMax)
        return false;
    }
  }

  const float EPS = 0.001f;
  return (tMin >= -EPS && tMin <= 1.0f + EPS);
}

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

      SphereCollider *bulletSphere =
          ECS_GET(world, b, SphereCollider, COMP_SPHERE_COLLIDER);

      float radius = bulletSphere->radius;

      for (uint32_t j = 0; j < enemyArch->count; j++) {
        entity_t enemy = enemyArch->entities[j];

        Active *enemyActive = ECS_GET(world, enemy, Active, COMP_ACTIVE);
        if (!enemyActive || !enemyActive->value)
          continue;

        CollisionInstance *ci =
            ECS_GET(world, enemy, CollisionInstance, COMP_COLLISION_INSTANCE);

        if (!ci)
          continue;

        // Layer filtering
        if (!(ci->collideMask & (1 << LAYER_BULLET)))
          continue;

        if (SweptSphereVsAABB(prevPos, nextPos, radius, ci->worldBounds)) {
          printf("HIT\n");
          pos->value = prevPos;
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
