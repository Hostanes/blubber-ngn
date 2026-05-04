#include "../game.h"
#include "systems.h"

#define MELEE_CHASE_SPEED    30.0f
#define MELEE_LUNGE_SPEED    50.0f
#define MELEE_WINDUP_TIME     0.3f
#define MELEE_LUNGE_MAX_TIME  0.55f
#define MELEE_RECOVER_TIME    0.2f
#define MELEE_TRIGGER_DIST   20.0f
#define MELEE_HIT_DIST        1.8f
#define MELEE_DAMAGE         25.0f
#define MELEE_REPATH_INTERVAL 3.0f
#define MELEE_ROTATE_SPEED   10.0f
#define MELEE_SEP_RADIUS      2.5f
#define MELEE_SEP_STRENGTH   12.0f

static void MeleeApplyDamage(world_t *world, entity_t target,
                              archetype_t *arch, float damage) {
  if (ArchetypeHas(arch, COMP_SHIELD)) {
    Shield *sh = ECS_GET(world, target, Shield, COMP_SHIELD);
    if (sh && sh->current > 0.0f) {
      sh->current -= damage;
      if (sh->current > 0.0f) return;
      sh->current = 0.0f;
    }
  }
  if (ArchetypeHas(arch, COMP_HEALTH)) {
    Health *hp = ECS_GET(world, target, Health, COMP_HEALTH);
    if (hp) {
      hp->current -= damage;
      if (hp->current <= 0.0f) TryKillEntity(world, target);
    }
  }
}

void EnemyMeleeAISystem(world_t *world, GameWorld *game,
                         archetype_t *arch, float dt) {
  Position    *playerPos  = ECS_GET(world, game->player, Position, COMP_POSITION);
  archetype_t *playerArch = WorldGetArchetype(world, game->playerArchId);
  if (!playerPos || !playerArch) return;

  for (uint32_t i = 0; i < arch->count; i++) {
    entity_t e = arch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value) continue;

    Position    *pos = ECS_GET(world, e, Position,    COMP_POSITION);
    Velocity    *vel = ECS_GET(world, e, Velocity,    COMP_VELOCITY);
    Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
    MeleeEnemy  *me  = ECS_GET(world, e, MeleeEnemy,  COMP_MELEE_ENEMY);
    if (!pos || !vel || !ori || !me) continue;

    float terrainY = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                                    pos->value.x, pos->value.z);

    Vector3 toPlayer = Vector3Subtract(playerPos->value, pos->value);
    toPlayer.y = 0.0f;
    float distXZ = Vector3Length(toPlayer);

    if (me->repathTimer > 0.0f) me->repathTimer -= dt;

    switch (me->state) {

    case MELEE_CHASING: {
      if (distXZ < MELEE_TRIGGER_DIST) {
        vel->value       = (Vector3){0, 0, 0};
        me->state        = MELEE_WINDING_UP;
        me->windupTimer  = MELEE_WINDUP_TIME;
        me->lungeTarget  = playerPos->value;
        me->lungeTarget.y = terrainY;
        break;
      }

      if (!me->pathPending) {
        NavPath *path = ECS_GET(world, e, NavPath, COMP_NAVPATH);
        bool arrived = EnemyFollowPath(world, game, e,
                                       MELEE_CHASE_SPEED, MELEE_ROTATE_SPEED, dt);

        if (arrived || (me->repathTimer <= 0.0f && path && path->count == 0)) {
          // Request a new path toward the player
          if (path && !me->pathPending) {
            int cx, cy;
            Vector3 goal = playerPos->value;
            goal.y = terrainY;
            if (NavGrid_WorldToCell(&game->navGrid, goal, &cx, &cy) &&
                game->navGrid.cells[NavGrid_Index(&game->navGrid, cx, cy)].type
                    != NAV_CELL_WALL) {
              // Pass NULL for combat — flush will not try to set state
              EnemyPathQueue_Submit(&game->navGrid, pos->value, goal,
                                    path, &me->pathPending, NULL);
            }
            me->repathTimer = MELEE_REPATH_INTERVAL;
          }
        }
      }
      // else: path request in-flight, stand still this frame
    } break;

    case MELEE_WINDING_UP: {
      pos->value.y = terrainY;
      vel->value   = (Vector3){0, 0, 0};
      if (distXZ > 0.001f)
        ori->yaw = atan2f(toPlayer.x / distXZ, toPlayer.z / distXZ);

      me->windupTimer -= dt;
      if (me->windupTimer <= 0.0f) {
        Vector3 toLunge = Vector3Subtract(me->lungeTarget, pos->value);
        toLunge.y = 0.0f;
        float d = Vector3Length(toLunge);
        if (d > 0.001f) {
          Vector3 dir = Vector3Scale(toLunge, 1.0f / d);
          vel->value.x = dir.x * MELEE_LUNGE_SPEED;
          vel->value.z = dir.z * MELEE_LUNGE_SPEED;
          vel->value.y = 4.0f;
        }
        me->state     = MELEE_LUNGING;
        me->lungeTimer = MELEE_LUNGE_MAX_TIME;
        me->hasHit     = false;
      }
    } break;

    case MELEE_LUNGING: {
      if (pos->value.y < terrainY) pos->value.y = terrainY;

      me->lungeTimer -= dt;

      if (!me->hasHit) {
        float distToPlayer = Vector3Distance(pos->value, playerPos->value);
        if (distToPlayer < MELEE_HIT_DIST) {
          MeleeApplyDamage(world, game->player, playerArch, MELEE_DAMAGE);
          me->hasHit = true;
        }
      }

      float distToTarget = Vector3Distance(
          (Vector3){pos->value.x, 0, pos->value.z},
          (Vector3){me->lungeTarget.x, 0, me->lungeTarget.z});

      if (me->lungeTimer <= 0.0f || distToTarget < 1.0f) {
        vel->value      = (Vector3){0, 0, 0};
        me->state        = MELEE_RECOVERING;
        me->recoverTimer = MELEE_RECOVER_TIME;
      }
    } break;

    case MELEE_RECOVERING: {
      pos->value.y = terrainY;
      vel->value   = (Vector3){0, 0, 0};
      me->recoverTimer -= dt;
      if (me->recoverTimer <= 0.0f) {
        me->state       = MELEE_CHASING;
        me->repathTimer = 0.0f; // trigger immediate repath
      }
    } break;
    }

    // Separation: push away from nearby melee siblings (skip during lunge)
    if (me->state != MELEE_LUNGING) {
      for (uint32_t j = 0; j < arch->count; j++) {
        if (j == i) continue;
        Active *oa = ECS_GET(world, arch->entities[j], Active, COMP_ACTIVE);
        if (!oa || !oa->value) continue;
        Position *opos = ECS_GET(world, arch->entities[j], Position, COMP_POSITION);
        if (!opos) continue;
        Vector3 diff = Vector3Subtract(pos->value, opos->value);
        diff.y = 0.0f;
        float dist = Vector3Length(diff);
        if (dist < MELEE_SEP_RADIUS && dist > 0.001f) {
          float push = (MELEE_SEP_RADIUS - dist) / MELEE_SEP_RADIUS;
          Vector3 nudge = Vector3Scale(Vector3Normalize(diff), push * MELEE_SEP_STRENGTH * dt);
          pos->value.x += nudge.x;
          pos->value.z += nudge.z;
        }
      }
    }
  }
}
