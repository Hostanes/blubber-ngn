#include "../components/components.h"
#include "../ecs_get.h"
#include "../game.h"
#include "../../engine/math/capsule.h"
#include "../../engine/math/collision_instance.h"
#include "raymath.h"

#define TARGET_RESPAWN_TIME 5.0f

static void ReviveTarget(world_t *world, entity_t e, TargetDummy *td) {
  Health *hp = ECS_GET(world, e, Health, COMP_HEALTH);
  Shield *sh = ECS_GET(world, e, Shield, COMP_SHIELD);
  if (hp) { hp->current = td->maxHealth; hp->max = td->maxHealth; }
  if (sh) { sh->current = td->maxShield; sh->max = td->maxShield; }

  Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
  if (pos) pos->value = td->spawnPos;

  Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
  if (ori) ori->yaw = td->spawnYaw;

  ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);
  if (mc) { for (int i = 0; i < mc->count; i++) mc->models[i].isActive = true; }

  CapsuleCollider *cap = ECS_GET(world, e, CapsuleCollider, COMP_CAPSULE_COLLIDER);
  CollisionInstance *ci = ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);
  if (cap && pos) Capsule_UpdateWorld(cap, pos->value);
  if (ci) {
    ci->layerMask   = 1 << LAYER_ENEMY;
    ci->collideMask = 1 << LAYER_BULLET;
  }

  td->respawnTimer = 0.0f;
  ECS_GET(world, e, Active, COMP_ACTIVE)->value = true;
}

void TargetDummySystem(world_t *world, GameWorld *game, float dt) {
  uint32_t archIds[2] = {game->targetStaticArchId, game->targetPatrolArchId};

  for (int ai = 0; ai < 2; ai++) {
    archetype_t *arch = WorldGetArchetype(world, archIds[ai]);
    if (!arch) continue;

    for (uint32_t i = 0; i < arch->count; i++) {
      entity_t e = arch->entities[i];

      Active      *active = ECS_GET(world, e, Active,       COMP_ACTIVE);
      TargetDummy *td     = ECS_GET(world, e, TargetDummy,  COMP_TARGET_DUMMY);
      if (!active || !td) continue;

      if (!active->value) {
        // Dead — tick respawn countdown
        if (td->respawnTimer > 0.0f) {
          td->respawnTimer -= dt;
          if (td->respawnTimer <= 0.0f) {
            // Also reset patrol state before reviving
            TargetPatrol *tp = ECS_GET(world, e, TargetPatrol, COMP_TARGET_PATROL);
            if (tp) { tp->t = 0.0f; tp->dir = 1; }
            ReviveTarget(world, e, td);
          }
        }
        continue;
      }

      // Alive — move patrol targets
      TargetPatrol *tp = ECS_GET(world, e, TargetPatrol, COMP_TARGET_PATROL);
      if (!tp) continue;

      float dist = Vector3Distance(tp->pointA, tp->pointB);
      if (dist < 0.01f) continue;

      float step = (tp->speed / dist) * dt;
      tp->t += (float)tp->dir * step;

      if (tp->t >= 1.0f) { tp->t = 1.0f; tp->dir = -1; }
      if (tp->t <= 0.0f) { tp->t = 0.0f; tp->dir =  1; }

      Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
      if (pos) {
        pos->value = Vector3Lerp(tp->pointA, tp->pointB, tp->t);
        CapsuleCollider   *cap = ECS_GET(world, e, CapsuleCollider,   COMP_CAPSULE_COLLIDER);
        CollisionInstance *ci  = ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);
        if (cap && ci) { Capsule_UpdateWorld(cap, pos->value); ci->worldBounds = Capsule_ComputeAABB(cap); }
      }
    }
  }
}
