#include "../game.h"
#include "systems.h"
#include <math.h>

#define DRONE_HOVER_HEIGHT       3.0f
#define DRONE_SPRING             9.0f
#define DRONE_DRAG               5.5f
#define DRONE_REGEN_RATE         20.0f   // shield/s given to ally
#define DRONE_REGEN_RADIUS       5.0f   // XZ distance to regen
#define DRONE_BONUS_SHIELD_MAX  60.0f   // cap for enemies with no innate shield
#define DRONE_YAW_SPEED          5.0f
#define DRONE_BOB_FREQ           1.2f
#define DRONE_BOB_AMP            0.25f

void EnemyDroneAISystem(world_t *world, GameWorld *game,
                         archetype_t *arch, float dt) {
  if (!arch) return;

  uint32_t allyArchIds[] = {
    game->enemyGruntArchId,
    game->enemyRangerArchId,
    game->enemyMeleeArchId,
  };

  for (uint32_t i = 0; i < arch->count; i++) {
    entity_t e = arch->entities[i];
    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value) continue;

    Position    *pos = ECS_GET(world, e, Position,    COMP_POSITION);
    Velocity    *vel = ECS_GET(world, e, Velocity,    COMP_VELOCITY);
    Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
    DroneEnemy  *dr  = ECS_GET(world, e, DroneEnemy,  COMP_DRONE_ENEMY);
    if (!pos || !vel || !ori || !dr) continue;

    dr->bobTimer       += dt;
    dr->retargetTimer  -= dt;

    // Validate existing target
    bool targetValid = false;
    if (dr->hasTarget) {
      Active *ta = ECS_GET(world, dr->target, Active, COMP_ACTIVE);
      targetValid = (ta && ta->value);
    }

    // Scan for a new target when needed
    if (!targetValid || dr->retargetTimer <= 0.0f) {
      float    bestDist = 1e9f;
      entity_t bestEnt  = {0};
      bool     found    = false;

      for (int ai = 0; ai < 3; ai++) {
        archetype_t *allyArch = WorldGetArchetype(world, allyArchIds[ai]);
        if (!allyArch) continue;
        for (uint32_t j = 0; j < allyArch->count; j++) {
          entity_t ally = allyArch->entities[j];
          Active *aa = ECS_GET(world, ally, Active, COMP_ACTIVE);
          if (!aa || !aa->value) continue;
          Position *ap = ECS_GET(world, ally, Position, COMP_POSITION);
          if (!ap) continue;
          float dx = ap->value.x - pos->value.x;
          float dz = ap->value.z - pos->value.z;
          float dist = sqrtf(dx*dx + dz*dz);
          if (dist < bestDist) { bestDist = dist; bestEnt = ally; found = true; }
        }
      }

      dr->hasTarget     = found;
      dr->target        = found ? bestEnt : (entity_t){0};
      dr->retargetTimer = 3.0f;
      targetValid       = found;
    }

    float terrainY = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                                    pos->value.x, pos->value.z);
    float bob      = sinf(dr->bobTimer * DRONE_BOB_FREQ) * DRONE_BOB_AMP;
    float desiredY = terrainY + DRONE_HOVER_HEIGHT + bob;

    Vector3 desired = pos->value;  // default: hover in place

    if (targetValid) {
      Position *tpos = ECS_GET(world, dr->target, Position, COMP_POSITION);
      if (!tpos) { dr->hasTarget = false; goto move; }
      desired = (Vector3){tpos->value.x, desiredY, tpos->value.z};

      // Face the ally
      float dx = tpos->value.x - pos->value.x;
      float dz = tpos->value.z - pos->value.z;
      if (dx*dx + dz*dz > 0.01f) {
        float targetYaw = atan2f(dx, dz);
        float diff = targetYaw - ori->yaw;
        while (diff >  (float)PI) diff -= 2.0f * (float)PI;
        while (diff < -(float)PI) diff += 2.0f * (float)PI;
        ori->yaw += diff * DRONE_YAW_SPEED * dt;
      }

      // Regen ally shield when close (works even if enemy has no innate shield)
      float xzDist = sqrtf((tpos->value.x - pos->value.x)*(tpos->value.x - pos->value.x) +
                           (tpos->value.z - pos->value.z)*(tpos->value.z - pos->value.z));
      if (xzDist < DRONE_REGEN_RADIUS) {
        Shield *sh = ECS_GET(world, dr->target, Shield, COMP_SHIELD);
        if (sh) {
          float cap = sh->max > 0.0f ? sh->max : DRONE_BONUS_SHIELD_MAX;
          sh->current += DRONE_REGEN_RATE * dt;
          if (sh->current > cap) sh->current = cap;
        }
      }
    } else {
      desired.y = desiredY;
    }

move:;
    // Spring toward desired position, with drag
    Vector3 toDesired = Vector3Subtract(desired, pos->value);
    vel->value = Vector3Add(vel->value, Vector3Scale(toDesired, DRONE_SPRING * dt));
    vel->value = Vector3Scale(vel->value, 1.0f - DRONE_DRAG * dt);

    pos->value = Vector3Add(pos->value, Vector3Scale(vel->value, dt));

    // Never clip below terrain
    if (pos->value.y < terrainY + 0.5f) pos->value.y = terrainY + 0.5f;
  }
}
