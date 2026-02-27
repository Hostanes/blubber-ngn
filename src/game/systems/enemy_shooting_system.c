#include "../game.h"
#include "enemy_behaviour.h"
#include "systems.h"

#define ENEMY_AIM_THRESHOLD 0

void MuzzleCollection_UpdateWorld(Position *pos, Orientation *entityOri,
                                  MuzzleCollection_t *muzzles) {
  if (!pos || !entityOri || !muzzles)
    return;

  for (int i = 0; i < muzzles->count; i++) {
    Muzzle_t *m = &muzzles->Muzzles[i];
    Vector3 offset = m->positionOffset.value;

    /* --- 1. Compute pivot (x,y,0) rotated by entity yaw --- */
    float ey = entityOri->yaw;
    float cey = cosf(ey);
    float sey = sinf(ey);

    Vector3 pivotLocal = {offset.x, offset.y, 0};

    Vector3 pivotWorld = {pivotLocal.x * cey - pivotLocal.z * sey, pivotLocal.y,
                          pivotLocal.x * sey + pivotLocal.z * cey};

    pivotWorld = Vector3Add(pos->value, pivotWorld);

    /* --- 2. Compute final world rotation --- */
    m->worldRot.yaw = entityOri->yaw + m->aimRot.yaw + m->weaponOffset.yaw;

    m->worldRot.pitch = m->aimRot.pitch + m->weaponOffset.pitch;

    float yaw = m->worldRot.yaw;
    float pitch = m->worldRot.pitch;

    /* --- 3. Compute forward --- */
    Vector3 forward = {cosf(pitch) * sinf(yaw), sinf(pitch),
                       cosf(pitch) * cosf(yaw)};

    m->forward = Vector3Normalize(forward);

    /* --- 4. Place muzzle --- */
    m->worldPosition =
        Vector3Add(pivotWorld, Vector3Scale(m->forward, offset.z));
  }
}

void EnemyAimSystem(world_t *world, GameWorld *game, archetype_t *enemyArch,
                    float dt) {
  Position *playerPos = ECS_GET(world, game->player, Position, COMP_POSITION);
  if (!playerPos)
    return;

  Vector3 playerAimPos = playerPos->value;
  playerAimPos.y -= 0.5; // Height adjustment

  for (uint32_t i = 0; i < enemyArch->count; i++) {
    entity_t e = enemyArch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value)
      continue;

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
    MuzzleCollection_t *muzzles =
        ECS_GET(world, e, MuzzleCollection_t, COMP_MUZZLES);
    CombatState_t *combat = ECS_GET(world, e, CombatState_t, COMP_COMBAT_STATE);

    if (!pos || !ori || !muzzles || !combat)
      continue;

    /* --- 1. Rotate Body toward Player ONLY in Combat State --- */
    if (combat->state == ENEMY_STATE_COMBAT) {
      Vector3 toPlayer = Vector3Subtract(playerAimPos, pos->value);
      float targetYaw = atan2f(toPlayer.x, toPlayer.z);

      float delta = targetYaw - ori->yaw;
      while (delta > PI)
        delta -= 2 * PI;
      while (delta < -PI)
        delta += 2 * PI;

      // Rotation speed for the body when aiming
      float step = 5.0f * dt;
      if (fabsf(delta) < step)
        ori->yaw = targetYaw;
      else
        ori->yaw += (delta > 0 ? 1 : -1) * step;
    }

    /* --- 2. Update Muzzle Swivels (Local Rotations) --- */
    for (int m = 0; m < muzzles->count; m++) {
      Muzzle_t *mz = &muzzles->Muzzles[m];

      // Calculate the world direction needed to hit the player from the
      // muzzle's pivot
      Vector3 toPlayer = Vector3Subtract(playerAimPos, pos->value);
      float targetWorldYaw = atan2f(toPlayer.x, toPlayer.z);

      float horizontalDist =
          sqrtf(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);
      float targetPitch =
          atan2f(toPlayer.y - mz->positionOffset.value.y, horizontalDist);

      // Convert world target yaw to local yaw relative to the current body
      // orientation
      float targetLocalYaw = targetWorldYaw - ori->yaw;
      while (targetLocalYaw > PI)
        targetLocalYaw -= 2 * PI;
      while (targetLocalYaw < -PI)
        targetLocalYaw += 2 * PI;

      // Smoothly interpolate the muzzle's local aimRot
      float swivelSpeed = 8.0f * dt;

      // Interpolate Yaw
      float yawDelta = targetLocalYaw - mz->aimRot.yaw;
      while (yawDelta > PI)
        yawDelta -= 2 * PI;
      while (yawDelta < -PI)
        yawDelta += 2 * PI;

      if (fabsf(yawDelta) < swivelSpeed)
        mz->aimRot.yaw = targetLocalYaw;
      else
        mz->aimRot.yaw += (yawDelta > 0 ? 1 : -1) * swivelSpeed;

      // Interpolate Pitch
      float pitchDelta = targetPitch - mz->aimRot.pitch;
      if (fabsf(pitchDelta) < swivelSpeed)
        mz->aimRot.pitch = targetPitch;
      else
        mz->aimRot.pitch += (pitchDelta > 0 ? 1 : -1) * swivelSpeed;
    }

    /* --- 3. Compute final matrices/vectors --- */
    MuzzleCollection_UpdateWorld(pos, ori, muzzles);
  }
}

// for grunts
void EnemyFireSystem(world_t *world, GameWorld *game, archetype_t *enemyArch) {
  Position *playerPos = ECS_GET(world, game->player, Position, COMP_POSITION);
  Vector3 playerAimPos = playerPos->value;
  playerAimPos.y -= 3; // Match the height used in AimSystem

  for (uint32_t i = 0; i < enemyArch->count; i++) {
    entity_t e = enemyArch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value)
      continue;

    CombatState_t *combat = ECS_GET(world, e, CombatState_t, COMP_COMBAT_STATE);

    if (!combat || combat->state != ENEMY_STATE_COMBAT)
      continue;

    Velocity *vel = ECS_GET(world, e, Velocity, COMP_VELOCITY);
    if (!vel)
      continue;

    float speedSq = vel->value.x * vel->value.x + vel->value.z * vel->value.z;
    if (speedSq > 0.1f)
      continue;

    Timer *fireTimer = ECS_GET(world, e, Timer, COMP_GRUNT_FIRE_TIMER);
    if (!fireTimer || fireTimer->value > 0.0f)
      continue;

    MuzzleCollection_t *muzzles =
        ECS_GET(world, e, MuzzleCollection_t, COMP_MUZZLES);
    if (!muzzles || muzzles->count == 0)
      continue;

    /* --- New: Aim Validation --- */
    bool isAimingProperly = false;
    for (int m = 0; m < muzzles->count; m++) {
      Muzzle_t *mz = &muzzles->Muzzles[m];

      // Direction from the muzzle to the player
      Vector3 dirToPlayer =
          Vector3Normalize(Vector3Subtract(playerAimPos, mz->worldPosition));

      // How well does the muzzle forward align with the direction to player?
      float aimAccuracy = Vector3DotProduct(mz->forward, dirToPlayer);

      if (aimAccuracy > ENEMY_AIM_THRESHOLD) {
        isAimingProperly = true;
        break;
      }
    }

    if (!isAimingProperly)
      continue; // Skip firing if not lined up

    /* Fire all muzzles */
    for (int m = 0; m < muzzles->count; m++) {
      FireMuzzle(world, game, e, game->enemyGruntArchId, &muzzles->Muzzles[m]);
    }

    fireTimer->value = 1.0f;
  }
}
