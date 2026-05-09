

#include "../game.h"
#include "../level_creater_helper.h"
#include "systems.h"
#include <stdint.h>

// Shield absorbs first (scaled by shieldMult). If shield survives, health is
// fully protected. If shield breaks, health takes the full healthMult hit.
static void ApplyDamage(world_t *world, entity_t target, archetype_t *arch,
                        float damage, float shieldMult, float healthMult) {
  if (ArchetypeHas(arch, COMP_SHIELD)) {
    Shield *sh = ECS_GET(world, target, Shield, COMP_SHIELD);
    if (sh && sh->current > 0.0f) {
      sh->current -= damage * shieldMult;
      if (sh->current > 0.0f)
        return; // shield held — health fully protected
      sh->current = 0.0f;
    }
  }
  if (ArchetypeHas(arch, COMP_HEALTH)) {
    Health *hp = ECS_GET(world, target, Health, COMP_HEALTH);
    if (hp) {
      hp->current -= damage * healthMult;
      if (hp->current <= 0.0f)
        TryKillEntity(world, target);
    }
  }
}

static void SpawnOneBullet(world_t *world, GameWorld *game, entity_t shooter,
                           int shooterArchId, Muzzle_t *m, Vector3 forward) {
  archetype_t *bulletArch = WorldGetArchetype(world, game->bulletArchId);

  for (uint32_t i = 0; i < bulletArch->count; i++) {
    entity_t b = bulletArch->entities[i];

    Active *active = ECS_GET(world, b, Active, COMP_ACTIVE);
    if (active->value)
      continue;

    active->value = true;

    BulletType *bt  = ECS_GET(world, b, BulletType, COMP_BULLETTYPE);
    bt->type       = m->bulletType;
    bt->shieldMult = m->shieldMult != 0.0f ? m->shieldMult : 1.0f;
    bt->healthMult = m->healthMult != 0.0f ? m->healthMult : 1.0f;
    bt->pierce     = m->pierce;

    float mv = muzzleVelocities[bt->type];

    ECS_GET(world, b, Position, COMP_POSITION)->value = m->worldPosition;
    ECS_GET(world, b, Velocity, COMP_VELOCITY)->value = Vector3Scale(forward, mv);

    Orientation *bori = ECS_GET(world, b, Orientation, COMP_ORIENTATION);
    bori->yaw   = atan2f(forward.x, forward.z);
    bori->pitch = asinf(Clamp(forward.y, -1.0f, 1.0f));

    ModelCollection_t *bmc = ECS_GET(world, b, ModelCollection_t, COMP_MODEL);
    bmc->models[0].rotation = (Vector3){-bori->pitch, 0.0f, 0.0f};

    BulletOwner *owner = ECS_GET(world, b, BulletOwner, COMP_BULLET_OWNER);
    owner->eId   = shooter.id;
    owner->archId = shooterArchId;

    Timer *life   = ECS_GET(world, b, Timer, COMP_TIMER);
    life->value   = 5.0f;

    CollisionInstance *ci = ECS_GET(world, b, CollisionInstance, COMP_COLLISION_INSTANCE);
    ci->layerMask   = 1 << LAYER_BULLET;
    ci->collideMask = (shooterArchId == game->playerArchId)
                          ? ((1 << LAYER_ENEMY) | (1 << LAYER_WORLD))
                          : ((1 << LAYER_PLAYER) | (1 << LAYER_WORLD));
    break;
  }
}

void FireMuzzle(world_t *world, GameWorld *game, entity_t shooter,
                int shooterArchId, Muzzle_t *m) {
  if (m->bulletType == BULLET_TYPE_MISSILE) {
    SpawnHomingMissile(world, game, shooter, game->player,
                       m->worldPosition, m->forward, true, 4.0f);
    return;
  }

  int pellets = m->spreadCount > 0 ? m->spreadCount : 1;

  float baseYaw   = atan2f(m->forward.x, m->forward.z);
  float basePitch = asinf(Clamp(m->forward.y, -1.0f, 1.0f));

  for (int s = 0; s < pellets; s++) {
    Vector3 fwd = m->forward;

    if (m->spreadAngle > 0.0f) {
      float yaw   = baseYaw   + ((float)GetRandomValue(-1000, 1000) / 1000.0f) * m->spreadAngle;
      float pitch = basePitch + ((float)GetRandomValue(-1000, 1000) / 1000.0f) * m->spreadAngle;
      fwd = (Vector3){cosf(pitch) * sinf(yaw), sinf(pitch), cosf(pitch) * cosf(yaw)};
    }

    SpawnOneBullet(world, game, shooter, shooterArchId, m, fwd);
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
  archetype_t *playerArch       = WorldGetArchetype(world, game->playerArchId);
  archetype_t *enemyArch        = WorldGetArchetype(world, game->enemyGruntArchId);
  archetype_t *rangerArch       = WorldGetArchetype(world, game->enemyRangerArchId);
  archetype_t *meleeArch        = WorldGetArchetype(world, game->enemyMeleeArchId);
  archetype_t *droneArch        = WorldGetArchetype(world, game->enemyDroneArchId);
  archetype_t *targetStaticArch = WorldGetArchetype(world, game->targetStaticArchId);
  archetype_t *targetPatrolArch = WorldGetArchetype(world, game->targetPatrolArchId);
  archetype_t *obstacleArch     = WorldGetArchetype(world, game->obstacleArchId);
  archetype_t *wallSegArch      = WorldGetArchetype(world, game->wallSegArchId);

  Position *playerPos = ECS_GET(world, game->player, Position, COMP_POSITION);
  Vector3 playerSoundPos = playerPos ? playerPos->value : (Vector3){0,0,0};

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
#define CHECK_ARCH(archPtr, hitSound, soundPos)                                \
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
      ApplyDamage(world, target, archPtr, bulletDamages[bulletType->type],      \
                  bulletType->shieldMult, bulletType->healthMult);             \
                                                                               \
      QueueSound(&game->soundSystem, hitSound, soundPos, 0.2f, 1.0f);         \
      hit = true;                                                              \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  if (hit)                                                                     \
    continue;

    /* --- Collision checks --- */
    CHECK_ARCH(playerArch,       SOUND_HITMARKER, playerSoundPos);
    CHECK_ARCH(enemyArch,        SOUND_HITMARKER, playerSoundPos);
    CHECK_ARCH(rangerArch,       SOUND_HITMARKER, playerSoundPos);
    CHECK_ARCH(meleeArch,        SOUND_HITMARKER, playerSoundPos);
    CHECK_ARCH(droneArch,        SOUND_HITMARKER, playerSoundPos);
    CHECK_ARCH(targetStaticArch, SOUND_HITMARKER, playerSoundPos);
    CHECK_ARCH(targetPatrolArch, SOUND_HITMARKER, playerSoundPos);
    CHECK_ARCH(obstacleArch,     SOUND_CLANG,     prevPos);

    // Wall segments: bidirectional collideMask check so blockProjectiles works
    for (uint32_t j = 0; j < wallSegArch->count; j++) {
      entity_t target = wallSegArch->entities[j];
      if (owner && target.id == owner->eId && wallSegArch->id == owner->archId)
        continue;
      Active *targetActive = ECS_GET(world, target, Active, COMP_ACTIVE);
      if (!targetActive || !targetActive->value) continue;
      CollisionInstance *ci = ECS_GET(world, target, CollisionInstance, COMP_COLLISION_INSTANCE);
      if (!ci) continue;
      CollisionInstance *bulletCI = ECS_GET(world, b, CollisionInstance, COMP_COLLISION_INSTANCE);
      if (!(bulletCI->collideMask & ci->layerMask)) continue;
      if (!(ci->collideMask & bulletCI->layerMask)) continue;
      if (SweptSphereVsAABB(prevPos, nextPos, radius, ci->worldBounds)) {
        active->value = false;
        pos->value    = prevPos;
        ApplyDamage(world, target, wallSegArch, bulletDamages[bulletType->type],
                    bulletType->shieldMult, bulletType->healthMult);
        QueueSound(&game->soundSystem, SOUND_CLANG, prevPos, 0.2f, 1.0f);
        hit = true;
        break;
      }
    }
    if (hit) continue;

#undef CHECK_ARCH

    if (!hit)
      pos->value = nextPos;
  }
}

static void SpawnExplosion(world_t *world, GameWorld *game, Vector3 center,
                           float maxDamage) {
  const float blastRadius = 8.0f;

  QueueSound(&game->soundSystem, SOUND_EXPLOSION, center, 1.0f, 1.0f);

  SpawnParticle(world, game, center, (Vector3){0.0f, 3.0f, 0.0f},
                blastRadius * 0.7f, 0.45f, (Color){255, 120, 30, 200});
  SpawnParticle(world, game, center, (Vector3){0.0f, 1.5f, 0.0f},
                blastRadius * 0.5f, 1.1f,  (Color){80, 70, 70, 150});

  archetype_t *archs[] = {
      WorldGetArchetype(world, game->playerArchId),
      WorldGetArchetype(world, game->enemyGruntArchId),
      WorldGetArchetype(world, game->enemyRangerArchId),
      WorldGetArchetype(world, game->enemyMeleeArchId),
      WorldGetArchetype(world, game->enemyDroneArchId),
      WorldGetArchetype(world, game->targetStaticArchId),
      WorldGetArchetype(world, game->targetPatrolArchId),
  };
  for (int t = 0; t < 7; t++) {
    archetype_t *arch = archs[t];
    if (!arch) continue;
    for (uint32_t i = 0; i < arch->count; i++) {
      entity_t ent    = arch->entities[i];
      Active  *active = ECS_GET(world, ent, Active, COMP_ACTIVE);
      if (!active || !active->value) continue;
      Position *epos = ECS_GET(world, ent, Position, COMP_POSITION);
      if (!epos) continue;
      float dist = Vector3Distance(center, epos->value);
      if (dist >= blastRadius) continue;
      float falloff = 1.0f - (dist / blastRadius);
      ApplyDamage(world, ent, arch, maxDamage * falloff, 1.0f, 1.0f);
    }
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

    Timer *life = ECS_GET(world, e, Timer, COMP_TIMER);
    if (life && life->value <= 0.0f) {
      StopLoopSound(&game->soundSystem, e);
      TryKillEntity(world, e);
      continue;
    }

    HomingMissile *hm = ECS_GET(world, e, HomingMissile, COMP_HOMINGMISSILE);
    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    Velocity *vel = ECS_GET(world, e, Velocity, COMP_VELOCITY);
    Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);

    if (!hm || !pos || !vel || !ori)
      continue;

    TickLoopSound(&game->soundSystem, LOOP_SOUND_ROCKET, e, pos->value, 0.9f, 1.0f);

    // Guided missiles home toward their target; unguided fly straight
    if (hm->guided) {
      Position *targetPos = ECS_GET(world, hm->target, Position, COMP_POSITION);
      if (targetPos) {
        Vector3 toTarget  = Vector3Subtract(targetPos->value, pos->value);
        float   distToTgt = Vector3Length(toTarget);

        if (!hm->armed && distToTgt < 20.0f)
          hm->armed = true;

        if (!hm->armed) {
          Vector3 desiredDir = Vector3Normalize(toTarget);
          Vector3 currentDir = Vector3Normalize(vel->value);
          Vector3 newDir     = Vector3Normalize(Vector3Lerp(currentDir, desiredDir, hm->turnSpeed * dt));
          float   speed      = Vector3Length(vel->value);
          if (speed > hm->maxSpeed) speed = hm->maxSpeed;
          vel->value = Vector3Scale(newDir, speed);
          ori->yaw   = atan2f(newDir.x, newDir.z);
          ori->pitch = asinf(newDir.y);
        }
      }
    }

    /* --- Model orientation toward movement direction --- */
    ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

    if (mc && mc->count > 0) {
      mc->models[0].rotation = (Vector3){-ori->pitch, 0.0f, 0.0f};
    }

    archetype_t *playerArch        = WorldGetArchetype(world, game->playerArchId);
    archetype_t *enemyArch         = WorldGetArchetype(world, game->enemyGruntArchId);
    archetype_t *rangerArch        = WorldGetArchetype(world, game->enemyRangerArchId);
    archetype_t *meleeArch         = WorldGetArchetype(world, game->enemyMeleeArchId);
    archetype_t *droneArch2        = WorldGetArchetype(world, game->enemyDroneArchId);
    archetype_t *targetStaticArch2 = WorldGetArchetype(world, game->targetStaticArchId);
    archetype_t *targetPatrolArch2 = WorldGetArchetype(world, game->targetPatrolArchId);
    archetype_t *obstacleArch      = WorldGetArchetype(world, game->obstacleArchId);
    archetype_t *wallSegArch       = WorldGetArchetype(world, game->wallSegArchId);

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
      SpawnExplosion(world, game, prevPos, hm->blastDamage);
      StopLoopSound(&game->soundSystem, e);
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
      SpawnExplosion(world, game, prevPos, hm->blastDamage);                  \
      StopLoopSound(&game->soundSystem, e);                                    \
      TryKillEntity(world, e);                                                 \
      pos->value = prevPos;                                                    \
      hit = true;                                                              \
      break;                                                                   \
    }                                                                          \
  }                                                                            \
  if (hit)                                                                     \
    continue;

    CHECK_ARCH(playerArch);
    CHECK_ARCH(enemyArch);
    CHECK_ARCH(rangerArch);
    CHECK_ARCH(meleeArch);
    CHECK_ARCH(droneArch2);
    CHECK_ARCH(targetStaticArch2);
    CHECK_ARCH(targetPatrolArch2);
    CHECK_ARCH(obstacleArch);

    // Wall segments: bidirectional collideMask check so blockProjectiles works
    for (uint32_t j = 0; j < wallSegArch->count; j++) {
      entity_t target = wallSegArch->entities[j];
      if (target.id == e.id || target.id == hm->owner.id) continue;
      Active *targetActive = ECS_GET(world, target, Active, COMP_ACTIVE);
      if (!targetActive || !targetActive->value) continue;
      CollisionInstance *ci = ECS_GET(world, target, CollisionInstance, COMP_COLLISION_INSTANCE);
      if (!ci) continue;
      if (!(missileCI->collideMask & ci->layerMask)) continue;
      if (!(ci->collideMask & missileCI->layerMask)) continue;
      if (SweptSphereVsAABB(prevPos, nextPos, radius, ci->worldBounds)) {
        SpawnExplosion(world, game, prevPos, hm->blastDamage);
        StopLoopSound(&game->soundSystem, e);
        TryKillEntity(world, e);
        pos->value = prevPos;
        hit = true;
        break;
      }
    }
    if (hit) continue;

#undef CHECK_ARCH

    if (!hit)
      pos->value = nextPos;
  }
}
