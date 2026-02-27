#include "../game.h"
#include "enemy_behaviour.h"
#include "systems.h"

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
  Position playerAimPos = *playerPos;
  playerAimPos.value.y -= 3;

  for (uint32_t i = 0; i < enemyArch->count; i++) {
    entity_t e = enemyArch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value)
      continue;

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);

    Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);

    MuzzleCollection_t *muzzles =
        ECS_GET(world, e, MuzzleCollection_t, COMP_MUZZLES);

    if (!pos || !ori || !muzzles || muzzles->count == 0)
      continue;

    for (int m = 0; m < muzzles->count; m++) {
      Muzzle_t *mz = &muzzles->Muzzles[m];

      Vector3 toPlayer = Vector3Subtract(playerAimPos.value, pos->value);

      float aimYaw = atan2f(toPlayer.x, toPlayer.z);

      float horizontal =
          sqrtf(toPlayer.x * toPlayer.x + toPlayer.z * toPlayer.z);

      float aimPitch = atan2f(toPlayer.y, horizontal);

      mz->aimRot.yaw = aimYaw - ori->yaw;
      mz->aimRot.pitch = aimPitch;
    }

    /* AFTER aim is computed */
    MuzzleCollection_UpdateWorld(pos, ori, muzzles);
  }
}

// for grunts
void EnemyFireSystem(world_t *world, GameWorld *game, archetype_t *enemyArch) {
  for (uint32_t i = 0; i < enemyArch->count; i++) {
    entity_t e = enemyArch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value)
      continue;

    Timer *fireTimer = ECS_GET(world, e, Timer, COMP_GRUNT_FIRE_TIMER);

    if (!fireTimer || fireTimer->value > 0.0f)
      continue;

    MuzzleCollection_t *muzzles =
        ECS_GET(world, e, MuzzleCollection_t, COMP_MUZZLES);

    if (!muzzles || muzzles->count == 0)
      continue;

    /* Fire all muzzles */
    for (int m = 0; m < muzzles->count; m++) {
      FireMuzzle(world, game, e, game->enemyGruntArchId, &muzzles->Muzzles[m]);
    }

    /* Reset fire timer */
    fireTimer->value = 1.0f; // adjust to desired fire rate
  }
}
