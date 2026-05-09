#include "../game.h"
#include "enemy_behaviour.h"
#include "systems.h"

#define RANGER_FIRE_DEBUG 0
#if RANGER_FIRE_DEBUG
#define RF_LOG(...) printf(__VA_ARGS__)
#else
#define RF_LOG(...)
#endif

void MuzzleCollection_UpdateWorld(Position *pos, Orientation *entityOri,
                                  MuzzleCollection_t *muzzles) {
  if (!pos || !entityOri || !muzzles) return;

  for (int i = 0; i < muzzles->count; i++) {
    Muzzle_t *m = &muzzles->Muzzles[i];
    Vector3 offset = m->positionOffset.value;

    float ey  = entityOri->yaw;
    float cey = cosf(ey), sey = sinf(ey);

    Vector3 pivotLocal = {offset.x, offset.y, 0};
    Vector3 pivotWorld = {pivotLocal.x * cey - pivotLocal.z * sey, pivotLocal.y,
                          pivotLocal.x * sey + pivotLocal.z * cey};
    pivotWorld = Vector3Add(pos->value, pivotWorld);

    m->worldRot.yaw   = entityOri->yaw + m->aimRot.yaw + m->weaponOffset.yaw;
    m->worldRot.pitch = m->aimRot.pitch + m->weaponOffset.pitch;

    float yaw   = m->worldRot.yaw;
    float pitch = m->worldRot.pitch;
    Vector3 forward = {cosf(pitch) * sinf(yaw), sinf(pitch),
                       cosf(pitch) * cosf(yaw)};
    m->forward = Vector3Normalize(forward);

    m->worldPosition = Vector3Add(pivotWorld, Vector3Scale(m->forward, offset.z));
  }
}

/* Helper: XZ body forward dot product vs player direction */
static float BodyDotToPlayer(Orientation *ori, Position *pos,
                              Vector3 playerAimPos) {
  Vector3 toP = Vector3Subtract(playerAimPos, pos->value);
  toP.y = 0.0f;
  float len = Vector3Length(toP);
  if (len < 0.001f) return 1.0f;
  toP = Vector3Scale(toP, 1.0f / len);
  Vector3 bodyFwd = {sinf(ori->yaw), 0.0f, cosf(ori->yaw)};
  return Vector3DotProduct(bodyFwd, toP);
}

static inline bool EnemyIsStationary(EnemyState_e s) {
  return s == ENEMY_STATE_COMBAT || s == ENEMY_AI_SUPPRESS || s == ENEMY_AI_COVER;
}
static inline bool EnemyCanFire(EnemyState_e s) {
  return s != ENEMY_STATE_IDLE && s != ENEMY_STATE_MOVING && s != ENEMY_AI_RETREAT;
}

/* ------------------------------------------------------------------ */
/*  Grunt aim                                                          */
/* ------------------------------------------------------------------ */

void EnemyAimSystem(world_t *world, GameWorld *game, archetype_t *enemyArch,
                    float dt) {
  Position *playerPos = ECS_GET(world, game->player, Position, COMP_POSITION);
  if (!playerPos) return;

  Vector3 playerAimPos = playerPos->value;
  playerAimPos.y -= 0.5f;

  for (uint32_t i = 0; i < enemyArch->count; i++) {
    entity_t e = enemyArch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value) continue;

    Position          *pos    = ECS_GET(world, e, Position,          COMP_POSITION);
    Orientation       *ori    = ECS_GET(world, e, Orientation,       COMP_ORIENTATION);
    MuzzleCollection_t *muzzles = ECS_GET(world, e, MuzzleCollection_t, COMP_MUZZLES);
    CombatState_t     *combat = ECS_GET(world, e, CombatState_t,     COMP_COMBAT_STATE);
    if (!pos || !ori || !muzzles || !combat) continue;

    // Rotate body toward player only while in combat
    if (EnemyIsStationary(combat->state)) {
      Vector3 toPlayer = Vector3Subtract(playerAimPos, pos->value);
      float targetYaw  = atan2f(toPlayer.x, toPlayer.z);
      float delta      = targetYaw - ori->yaw;
      while (delta >  PI) delta -= 2.0f * PI;
      while (delta < -PI) delta += 2.0f * PI;
      float step = bodyAimSpeeds[0] * dt;
      if (fabsf(delta) < step) ori->yaw = targetYaw;
      else                     ori->yaw += (delta > 0.0f ? 1.0f : -1.0f) * step;
    }

    // Swivel muzzles
    for (int m = 0; m < muzzles->count; m++) {
      Muzzle_t *mz = &muzzles->Muzzles[m];
      Vector3 toPlayer = Vector3Subtract(playerAimPos, pos->value);

      float targetWorldYaw  = atan2f(toPlayer.x, toPlayer.z);
      float horizontalDist  = sqrtf(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
      float targetPitch     = atan2f(toPlayer.y - mz->positionOffset.value.y,
                                     horizontalDist);
      float targetLocalYaw  = targetWorldYaw - ori->yaw;
      while (targetLocalYaw >  PI) targetLocalYaw -= 2.0f * PI;
      while (targetLocalYaw < -PI) targetLocalYaw += 2.0f * PI;

      float swivelSpeed = muzzleAimSpeeds[0] * dt;

      float yawDelta = targetLocalYaw - mz->aimRot.yaw;
      while (yawDelta >  PI) yawDelta -= 2.0f * PI;
      while (yawDelta < -PI) yawDelta += 2.0f * PI;
      if (fabsf(yawDelta) < swivelSpeed) mz->aimRot.yaw = targetLocalYaw;
      else                               mz->aimRot.yaw += (yawDelta > 0.0f ? 1.0f : -1.0f) * swivelSpeed;

      float pitchDelta = targetPitch - mz->aimRot.pitch;
      if (fabsf(pitchDelta) < swivelSpeed) mz->aimRot.pitch = targetPitch;
      else                                 mz->aimRot.pitch += (pitchDelta > 0.0f ? 1.0f : -1.0f) * swivelSpeed;
    }

    MuzzleCollection_UpdateWorld(pos, ori, muzzles);

    ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);
    if (mc && mc->count > 2)
      mc->models[2].rotation.x = -muzzles->Muzzles[0].aimRot.pitch;
  }
}

/* ------------------------------------------------------------------ */
/*  Grunt fire                                                         */
/* ------------------------------------------------------------------ */

void EnemyFireSystem(world_t *world, GameWorld *game, archetype_t *enemyArch) {
  Position *playerPos = ECS_GET(world, game->player, Position, COMP_POSITION);
  if (!playerPos) return;
  Vector3 playerAimPos = playerPos->value;
  playerAimPos.y -= 0.5f;

  for (uint32_t i = 0; i < enemyArch->count; i++) {
    entity_t e = enemyArch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value) continue;

    CombatState_t *combat = ECS_GET(world, e, CombatState_t, COMP_COMBAT_STATE);
    if (!combat || !EnemyCanFire(combat->state)) continue;

    // Settle: wait for body to rotate before firing
    if (combat->settleTimer > 0.0f) continue;

    // LOS: don't fire through walls
    if (!combat->hasLOS) continue;

    Timer *fireTimer = ECS_GET(world, e, Timer, COMP_GRUNT_FIRE_TIMER);
    if (!fireTimer || fireTimer->value > 0.0f) continue;

    Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
    Position    *pos = ECS_GET(world, e, Position,    COMP_POSITION);
    if (!ori || !pos) continue;

    // Body must be facing the player before firing
    if (BodyDotToPlayer(ori, pos, playerAimPos) < ENEMY_BODY_YAW_THRESHOLD)
      continue;

    MuzzleCollection_t *muzzles = ECS_GET(world, e, MuzzleCollection_t, COMP_MUZZLES);
    if (!muzzles || muzzles->count == 0) continue;

    // Muzzle must also be on-target
    bool aimed = false;
    for (int m = 0; m < muzzles->count; m++) {
      Muzzle_t *mz = &muzzles->Muzzles[m];
      Vector3 dirToPlayer = Vector3Normalize(
          Vector3Subtract(playerAimPos, mz->worldPosition));
      if (Vector3DotProduct(mz->forward, dirToPlayer) > ENEMY_AIM_THRESHOLD) {
        aimed = true;
        break;
      }
    }
    if (!aimed) continue;

    for (int m = 0; m < muzzles->count; m++)
      FireMuzzle(world, game, e, game->enemyGruntArchId, &muzzles->Muzzles[m]);

    fireTimer->value = 1.0f;
  }
}

/* ------------------------------------------------------------------ */
/*  Ranger aim                                                         */
/* ------------------------------------------------------------------ */

void EnemyRangerAimSystem(world_t *world, GameWorld *game,
                           archetype_t *enemyArch, float dt) {
  Position *playerPos = ECS_GET(world, game->player, Position, COMP_POSITION);
  if (!playerPos) return;

  Vector3 playerAimPos = playerPos->value;
  playerAimPos.y -= 0.5f;

  for (uint32_t i = 0; i < enemyArch->count; i++) {
    entity_t e = enemyArch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value) continue;

    Position          *pos     = ECS_GET(world, e, Position,           COMP_POSITION);
    Orientation       *ori     = ECS_GET(world, e, Orientation,        COMP_ORIENTATION);
    MuzzleCollection_t *muzzles = ECS_GET(world, e, MuzzleCollection_t, COMP_MUZZLES);
    CombatState_t     *combat  = ECS_GET(world, e, CombatState_t,      COMP_COMBAT_STATE);
    if (!pos || !ori || !muzzles || !combat) continue;

    if (EnemyIsStationary(combat->state)) {
      Vector3 toPlayer = Vector3Subtract(playerAimPos, pos->value);
      float targetYaw  = atan2f(toPlayer.x, toPlayer.z);
      float delta      = targetYaw - ori->yaw;
      while (delta >  PI) delta -= 2.0f * PI;
      while (delta < -PI) delta += 2.0f * PI;
      float step = bodyAimSpeeds[1] * dt;
      if (fabsf(delta) < step) ori->yaw = targetYaw;
      else                     ori->yaw += (delta > 0.0f ? 1.0f : -1.0f) * step;
    }

    for (int m = 0; m < muzzles->count; m++) {
      Muzzle_t *mz = &muzzles->Muzzles[m];

      if (mz->bulletType == BULLET_TYPE_MISSILE) {
        mz->aimRot.yaw   = 0.0f;
        mz->aimRot.pitch = PI / 2.0f;
        continue;
      }

      Vector3 toPlayer      = Vector3Subtract(playerAimPos, pos->value);
      float targetWorldYaw  = atan2f(toPlayer.x, toPlayer.z);
      float horizontalDist  = sqrtf(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
      float targetPitch     = atan2f(toPlayer.y - mz->positionOffset.value.y,
                                     horizontalDist);
      float targetLocalYaw  = targetWorldYaw - ori->yaw;
      while (targetLocalYaw >  PI) targetLocalYaw -= 2.0f * PI;
      while (targetLocalYaw < -PI) targetLocalYaw += 2.0f * PI;

      float swivelSpeed = muzzleAimSpeeds[1] * dt;

      float yawDelta = targetLocalYaw - mz->aimRot.yaw;
      while (yawDelta >  PI) yawDelta -= 2.0f * PI;
      while (yawDelta < -PI) yawDelta += 2.0f * PI;
      if (fabsf(yawDelta) < swivelSpeed) mz->aimRot.yaw = targetLocalYaw;
      else                               mz->aimRot.yaw += (yawDelta > 0.0f ? 1.0f : -1.0f) * swivelSpeed;

      float pitchDelta = targetPitch - mz->aimRot.pitch;
      if (fabsf(pitchDelta) < swivelSpeed) mz->aimRot.pitch = targetPitch;
      else                                 mz->aimRot.pitch += (pitchDelta > 0.0f ? 1.0f : -1.0f) * swivelSpeed;
    }

    MuzzleCollection_UpdateWorld(pos, ori, muzzles);

    ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);
    if (mc && mc->count > 2)
      mc->models[2].rotation.x = -muzzles->Muzzles[0].aimRot.pitch;
  }
}

/* ------------------------------------------------------------------ */
/*  Ranger fire                                                        */
/* ------------------------------------------------------------------ */

void EnemyRangerFireSystem(world_t *world, GameWorld *game,
                            archetype_t *enemyArch, float dt) {
  for (uint32_t i = 0; i < enemyArch->count; i++) {
    entity_t e = enemyArch->entities[i];
    RF_LOG("[RANGER FIRE] Checking entity %u\n", e);

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value) continue;

    CombatState_t *combat = ECS_GET(world, e, CombatState_t, COMP_COMBAT_STATE);
    if (!combat || !EnemyCanFire(combat->state)) continue;

    Timer *fireTimer = ECS_GET(world, e, Timer, COMP_GRUNT_FIRE_TIMER);
    if (!fireTimer) continue;

    MuzzleCollection_t *muzzles = ECS_GET(world, e, MuzzleCollection_t, COMP_MUZZLES);
    if (!muzzles || muzzles->count < 2) continue;

    /* 1. Currently in burst? */
    if (combat->burstShotsRemaining > 0) {
      combat->burstTimer -= dt;
      if (combat->burstTimer <= 0.0f) {
        Muzzle_t *mz = NULL;
        if (combat->burstType == 0) {
          mz = &muzzles->Muzzles[0];
        } else {
          mz = &muzzles->Muzzles[1];
          mz->forward = (Vector3){0, 1, 0};
        }
        FireMuzzle(world, game, e, game->enemyRangerArchId, mz);
        combat->burstShotsRemaining--;
        combat->burstTimer = 0.12f;
      }
      continue;
    }

    /* 2. Tick cooldown (was previously missing — bug fix) */
    fireTimer->value -= dt;
    if (fireTimer->value > 0.0f) continue;

    /* 3. Settle: body must have rotated before starting a burst */
    if (combat->settleTimer > 0.0f) continue;

    /* 4. Body-yaw alignment check */
    Position    *pos       = ECS_GET(world, e, Position,    COMP_POSITION);
    Position    *playerPos = ECS_GET(world, game->player, Position, COMP_POSITION);
    Orientation *ori       = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
    if (!pos || !playerPos || !ori) continue;

    Vector3 playerAimPos = playerPos->value;
    playerAimPos.y -= 0.5f;

    float bodyDot = BodyDotToPlayer(ori, pos, playerAimPos);

    /* 5. Aim check for autocannon */
    Muzzle_t *acMuzzle = &muzzles->Muzzles[0];
    Vector3 dirToPlayer = Vector3Normalize(
        Vector3Subtract(playerAimPos, acMuzzle->worldPosition));
    float aimAccuracy = Vector3DotProduct(acMuzzle->forward, dirToPlayer);

    /* 6. Roll for weapon type */
    int roll = GetRandomValue(0, 100);
    RF_LOG("  -> Roll: %d\n", roll);

    if (roll < 80) {
      // Autocannon: requires LOS, muzzle aim, AND body facing
      if (!combat->hasLOS) continue;
      if (aimAccuracy <= ENEMY_AIM_THRESHOLD) {
        RF_LOG("  -> Autocannon: muzzle not aimed (%.2f)\n", aimAccuracy);
        continue;
      }
      if (bodyDot < ENEMY_BODY_YAW_THRESHOLD) {
        RF_LOG("  -> Autocannon: body not facing (%.2f)\n", bodyDot);
        continue;
      }
      combat->burstShotsRemaining = GetRandomValue(3, 4);
      combat->burstType           = 0;
      combat->burstTimer          = 0.0f;
      fireTimer->value            = 1.2f;
    } else {
      // Missile: fires straight up regardless of aim direction
      combat->burstShotsRemaining = GetRandomValue(3, 4);
      combat->burstType           = 1;
      combat->burstTimer          = 0.0f;
      fireTimer->value            = 3.5f;
    }
  }
}
