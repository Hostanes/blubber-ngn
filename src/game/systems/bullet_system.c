

#include "../game.h"
#include "../level_creater_helper.h"
#include "systems.h"
#include <stdint.h>

void FireMuzzle(world_t *world, GameWorld *game, entity_t shooter,
                int shooterArchId, Muzzle_t *m) {
  archetype_t *bulletArch = WorldGetArchetype(world, game->bulletArchId);

  if (m->bulletType == BULLET_TYPE_MISSILE) {

    Vector3 muzzlePos = m->worldPosition;
    Vector3 forward = m->forward;

    SpawnHomingMissile(world, game, shooter,
                       game->player, // target
                       muzzlePos, forward);
    return;
  }

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

    // --- Collision mask ---
    CollisionInstance *ci =
        ECS_GET(world, b, CollisionInstance, COMP_COLLISION_INSTANCE);

    ci->layerMask = 1 << LAYER_BULLET;

    if (shooterArchId == game->playerArchId) {
      ci->collideMask = (1 << LAYER_ENEMY) | (1 << LAYER_WORLD);
    } else {
      ci->collideMask = (1 << LAYER_PLAYER) | (1 << LAYER_WORLD);
    }

    // printf("spawned bullet\n");
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
                  float dt) {
  archetype_t *playerArch = WorldGetArchetype(world, game->playerArchId);

  archetype_t *enemyArch = WorldGetArchetype(world, game->enemyGruntArchId);

  archetype_t *rangerArch = WorldGetArchetype(world, game->enemyRangerArchId);

  archetype_t *obstacleArch = WorldGetArchetype(world, game->obstacleArchId);
  archetype_t *wallSegArch  = WorldGetArchetype(world, game->wallSegArchId);

#pragma omp parallel for if (bulletArch->count >= OMP_MIN_ITERATIONS)
  for (uint32_t i = 0; i < bulletArch->count; i++) {
    entity_t b = bulletArch->entities[i];

    Active *active = ECS_GET(world, b, Active, COMP_ACTIVE);
    if (!active || !active->value)
      continue;

    Timer *life = ECS_GET(world, b, Timer, COMP_TIMER);
    if (!life || life->value <= 0.0f) {
      active->value = false;
      continue;
    }

    Position *pos = ECS_GET(world, b, Position, COMP_POSITION);
    Velocity *vel = ECS_GET(world, b, Velocity, COMP_VELOCITY);
    SphereCollider *bulletSphere =
        ECS_GET(world, b, SphereCollider, COMP_SPHERE_COLLIDER);
    BulletType *bulletType = ECS_GET(world, b, BulletType, COMP_BULLETTYPE);
    BulletOwner *owner = ECS_GET(world, b, BulletOwner, COMP_BULLET_OWNER);

    Vector3 prevPos = pos->value;
    Vector3 nextPos = Vector3Add(prevPos, Vector3Scale(vel->value, dt));
    Vector3 delta = Vector3Subtract(nextPos, prevPos);

    float terrainY = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                                   prevPos.x, prevPos.z);

    /* --- Terrain collision --- */
    if (prevPos.y <= terrainY) {
      active->value = false;
      continue;
    }

    float radius = bulletSphere->radius;

    bool hit = false;

    /* helper macro to check one archetype */
#define CHECK_ARCH(archPtr)                                                    \
  for (uint32_t j = 0; j < (archPtr)->count; j++) {                            \
    entity_t target = (archPtr)->entities[j];                                  \
                                                                               \
    if (owner && target.id == owner->eId && (archPtr)->id == owner->archId)    \
      continue;                                                                \
                                                                               \
    Active *targetActive = ECS_GET(world, target, Active, COMP_ACTIVE);        \
    if (!targetActive || !targetActive->value)                                 \
      continue;                                                                \
                                                                               \
    CollisionInstance *ci =                                                    \
        ECS_GET(world, target, CollisionInstance, COMP_COLLISION_INSTANCE);    \
    if (!ci)                                                                   \
      continue;                                                                \
    CollisionInstance *bulletCI =                                              \
        ECS_GET(world, b, CollisionInstance, COMP_COLLISION_INSTANCE);         \
                                                                               \
    if (!(bulletCI->collideMask & ci->layerMask))                              \
      continue;                                                                \
                                                                               \
    if (SweptSphereVsAABB(prevPos, nextPos, radius, ci->worldBounds)) {        \
      active->value = false;                                                   \
      pos->value = prevPos;                                                    \
                                                                               \
      if (ArchetypeHas(archPtr, COMP_HEALTH)) {                                \
        Health *hp = ECS_GET(world, target, Health, COMP_HEALTH);              \
        hp->current -= bulletDamages[bulletType->type];                        \
        if (hp->current <= 0.0f) {                                             \
          TryKillEntity(world, target);                                        \
        }                                                                      \
      }                                                                        \
                                                                               \
      hit = true;                                                              \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  if (hit)                                                                     \
    continue;

    /* --- Collision checks --- */
    CHECK_ARCH(playerArch);
    CHECK_ARCH(enemyArch);
    CHECK_ARCH(obstacleArch);
    CHECK_ARCH(rangerArch);
    CHECK_ARCH(wallSegArch);

#undef CHECK_ARCH

    if (!hit)
      pos->value = nextPos;
  }
}

void HomingMissileSystem(world_t *world, GameWorld *game, archetype_t *arch,
                         float dt) {

  for (uint32_t i = 0; i < arch->count; i++) {

    entity_t e = arch->entities[i];
    // printf("processing missle of eid %d\n", e.id);

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value)
      continue;

    HomingMissile *hm = ECS_GET(world, e, HomingMissile, COMP_HOMINGMISSILE);
    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    Velocity *vel = ECS_GET(world, e, Velocity, COMP_VELOCITY);
    Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);

    if (!hm || !pos || !vel || !ori)
      continue;

    Position *targetPos = ECS_GET(world, hm->target, Position, COMP_POSITION);

    if (!targetPos)
      continue;

    Vector3 toTarget = Vector3Subtract(targetPos->value, pos->value);

    Vector3 desiredDir = Vector3Normalize(toTarget);
    Vector3 currentDir = Vector3Normalize(vel->value);

    float maxTurn = hm->turnSpeed * dt;

    Vector3 newDir = Vector3Lerp(currentDir, desiredDir, maxTurn);
    newDir = Vector3Normalize(newDir);

    float speed = Vector3Length(vel->value);
    if (speed > hm->maxSpeed)
      speed = hm->maxSpeed;

    vel->value = Vector3Scale(newDir, speed);

    ori->yaw = atan2f(newDir.x, newDir.z);

    ori->pitch = asinf(newDir.y);

    /* --- Model orientation toward movement direction --- */
    ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

    if (mc && mc->count > 0) {
      mc->models[0].rotation = (Vector3){-ori->pitch, 0.0f, 0.0f};
    }

    archetype_t *playerArch  = WorldGetArchetype(world, game->playerArchId);
    archetype_t *enemyArch   = WorldGetArchetype(world, game->enemyGruntArchId);
    archetype_t *rangerArch  = WorldGetArchetype(world, game->enemyRangerArchId);
    archetype_t *obstacleArch = WorldGetArchetype(world, game->obstacleArchId);
    archetype_t *wallSegArch  = WorldGetArchetype(world, game->wallSegArchId);

    SphereCollider *missileSphere =
        ECS_GET(world, e, SphereCollider, COMP_SPHERE_COLLIDER);

    CollisionInstance *missileCI =
        ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);

    Vector3 prevPos = pos->value;
    Vector3 nextPos = Vector3Add(prevPos, Vector3Scale(vel->value, dt));

    float terrainY = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                                   prevPos.x, prevPos.z);

    /* --- Terrain collision --- */
    if (prevPos.y <= terrainY) {
      TryKillEntity(world, e);
      continue;
    }

    float radius = missileSphere->radius;

    bool hit = false;

#define CHECK_ARCH(archPtr)                                                    \
  for (uint32_t j = 0; j < (archPtr)->count; j++) {                            \
    entity_t target = (archPtr)->entities[j];                                  \
                                                                               \
    if (target.id == e.id)                                                     \
      continue;                                                                \
    if (target.id == hm->owner.id)                                             \
      continue;                                                                \
                                                                               \
    Active *targetActive = ECS_GET(world, target, Active, COMP_ACTIVE);        \
    if (!targetActive || !targetActive->value)                                 \
      continue;                                                                \
                                                                               \
    CollisionInstance *ci =                                                    \
        ECS_GET(world, target, CollisionInstance, COMP_COLLISION_INSTANCE);    \
    if (!ci)                                                                   \
      continue;                                                                \
                                                                               \
    if (!(missileCI->collideMask & ci->layerMask))                             \
      continue;                                                                \
                                                                               \
    if (SweptSphereVsAABB(prevPos, nextPos, radius, ci->worldBounds)) {        \
      TryKillEntity(world, e);                                                 \
      pos->value = prevPos;                                                    \
                                                                               \
      if (ArchetypeHas(archPtr, COMP_HEALTH)) {                                \
        Health *hp = ECS_GET(world, target, Health, COMP_HEALTH);              \
        hp->current -= 100.0f;                                                 \
        if (hp->current <= 0.0f)                                               \
          TryKillEntity(world, target);                                        \
      }                                                                        \
                                                                               \
      hit = true;                                                              \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  if (hit)                                                                     \
    continue;

    CHECK_ARCH(playerArch);
    CHECK_ARCH(enemyArch);
    CHECK_ARCH(obstacleArch);
    CHECK_ARCH(rangerArch);
    CHECK_ARCH(wallSegArch);

#undef CHECK_ARCH

    if (!hit)
      pos->value = nextPos;
  }
}
