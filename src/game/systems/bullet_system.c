
#include "../game.h"
#include "systems.h"
#include <stdint.h>

void FireMuzzle(world_t *world, GameWorld *game, entity_t shooter,
                int shooterArchId, Muzzle_t *m) {
  archetype_t *bulletArch = WorldGetArchetype(world, game->bulletArchId);

  for (uint32_t i = 0; i < bulletArch->count; i++) {
    entity_t b = bulletArch->entities[i];

    Active *active = ECS_GET(world, b, Active, COMP_ACTIVE);
    if (active->value)
      continue;

    active->value = true;

    BulletType *bt = ECS_GET(world, b, BulletType, COMP_BULLETTYPE);
    bt->type = m->bulletType;

    float muzzleVelocity = muzzleVelocities[bt->type];

    Vector3 muzzlePos = m->worldPosition;
    Vector3 forward = m->forward;

    // --- Position ---
    ECS_GET(world, b, Position, COMP_POSITION)->value = muzzlePos;

    // --- Velocity ---
    ECS_GET(world, b, Velocity, COMP_VELOCITY)->value =
        Vector3Scale(forward, muzzleVelocity);

    // --- Orientation ---
    Orientation *bori = ECS_GET(world, b, Orientation, COMP_ORIENTATION);

    bori->yaw = atan2f(forward.x, forward.z);
    bori->pitch = asinf(forward.y);

    // --- Model rotation ---
    ModelCollection_t *bmc = ECS_GET(world, b, ModelCollection_t, COMP_MODEL);

    bmc->models[0].rotation = (Vector3){-bori->pitch, 0.0f, 0.0f};

    // --- Owner ---
    BulletOwner *owner = ECS_GET(world, b, BulletOwner, COMP_BULLET_OWNER);

    owner->eId = shooter.id;
    owner->archId = shooterArchId;

    // --- Lifetime ---
    Timer *life = ECS_GET(world, b, Timer, COMP_TIMER);
    life->value = 5.0f;

    printf("spawned bullet\n");
    break;
  }
}

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

void BulletSystem(world_t *world, GameWorld *game, archetype_t *bulletArch,
                  archetype_t *enemyArch, float dt) {
  for (uint32_t i = 0; i < bulletArch->count; i++) {
    entity_t b = bulletArch->entities[i];

    Active *active = ECS_GET(world, b, Active, COMP_ACTIVE);
    if (!active->value)
      continue;

    Timer *life = ECS_GET(world, b, Timer, COMP_TIMER);
    if (life->value <= 0.0f) {
      active->value = false;
      printf("bullet died\n");
      continue;
    }

    Position *pos = ECS_GET(world, b, Position, COMP_POSITION);
    Velocity *vel = ECS_GET(world, b, Velocity, COMP_VELOCITY);

    Vector3 prevPos = pos->value;
    Vector3 nextPos = Vector3Add(prevPos, Vector3Scale(vel->value, dt));
    Vector3 delta = Vector3Subtract(nextPos, prevPos);

    float terrainY = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                                   pos->value.x, pos->value.z);

    BulletType *bulletType = ECS_GET(world, b, BulletType, COMP_BULLETTYPE);

    float length = Vector3Length(delta);

    if (length > 0.0001f) {
      Vector3 dir = Vector3Scale(delta, 1.0f / length);

      Ray ray = {prevPos, dir};

      SphereCollider *bulletSphere =
          ECS_GET(world, b, SphereCollider, COMP_SPHERE_COLLIDER);

      float radius = bulletSphere->radius;

      if (pos->value.y <= terrainY) {
        active->value = false;
        printf("bullet Terrain collision\n");
        // if (ArchetypeHas(enemyArch, COMP_NAVPATH)) {
        //   printf("--- Pathing to %f, %f\n", pos->value.x, pos->value.z);
        //   NavPath *navpath =
        //       ECS_GET(world, enemyArch->entities[0], NavPath, COMP_NAVPATH);
        //   Position *enemyPos =
        //       ECS_GET(world, enemyArch->entities[0], Position,
        //       COMP_POSITION);
        //   printf("Enemy start pos: %.2f %.2f\n", enemyPos->value.x,
        //          enemyPos->value.z);

        //   NavGrid_FindPath(&game->navGrid, enemyPos->value, pos->value,
        //                    navpath);
        // }
        continue;
      }

      BulletOwner *owner = ECS_GET(world, b, BulletOwner, COMP_BULLET_OWNER);

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

          // handle collision
          if (ArchetypeHas(enemyArch, COMP_HEALTH)) {
            Health *hp = ECS_GET(world, enemy, Health, COMP_HEALTH);
            hp->current -= bulletDamages[bulletType->type];
            if (hp->current <= 0.0f) {
              Active *active = ECS_GET(world, enemy, Active, COMP_ACTIVE);
              active->value = false;
              printf("Enemy died\n");
            }
          }
          break;
        }
      }
    }

    // Move bullet after sweep
    pos->value = nextPos;
  }
}
