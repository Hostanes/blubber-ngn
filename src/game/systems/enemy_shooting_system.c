#include "../game.h"
#include "systems.h"

void EnemyShootSystem(world_t *world, GameWorld *game, archetype_t *enemyArch,
                      float dt) {
  Position *playerPos = ECS_GET(world, game->player, Position, COMP_POSITION);

  if (!playerPos)
    return;

  archetype_t *bulletArch = WorldGetArchetype(world, game->bulletArchId);

  for (uint32_t i = 0; i < enemyArch->count; i++) {
    entity_t e = enemyArch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value)
      continue;

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
    Timer *cooldown = ECS_GET(world, e, Timer, COMP_GRUNT_FIRE_TIMER);

    if (!pos || !ori || !cooldown)
      continue;

    // Reduce cooldown
    cooldown->value -= dt;

    Vector3 toPlayer = Vector3Subtract(playerPos->value, pos->value);
    float distance = Vector3Length(toPlayer);

    // Only shoot within range
    if (distance > 60.0f)
      continue;

    // Face player
    Vector3 flatDir = toPlayer;
    flatDir.y = 0;

    if (Vector3Length(flatDir) > 0.001f) {
      flatDir = Vector3Normalize(flatDir);
      ori->yaw = atan2f(flatDir.x, flatDir.z);
    }

    // If still cooling down → skip
    if (cooldown->value > 0.0f)
      continue;

    // Reset cooldown
    cooldown->value = 1.5f; // fire every 1.5 sec

    // Fire bullet
    Vector3 forward = Vector3Normalize(toPlayer);
    Vector3 muzzlePos = Vector3Add(pos->value, (Vector3){0, 1.5f, 0});

    for (uint32_t j = 0; j < bulletArch->count; j++) {
      entity_t b = bulletArch->entities[j];
      Active *bActive = ECS_GET(world, b, Active, COMP_ACTIVE);

      if (bActive->value)
        continue;

      bActive->value = true;

      ECS_GET(world, b, Position, COMP_POSITION)->value = muzzlePos;
      ECS_GET(world, b, Velocity, COMP_VELOCITY)->value =
          Vector3Scale(forward, 40.0f);

      BulletOwner *owner = ECS_GET(world, b, BulletOwner, COMP_BULLET_OWNER);

      owner->eId = e.id;
      owner->archId = game->enemyCapsuleArchId;
      Orientation *bori = ECS_GET(world, b, Orientation, COMP_ORIENTATION);

      bori->yaw = atan2f(forward.x, forward.z);
      bori->pitch = asinf(forward.y);

      Timer *life = ECS_GET(world, b, Timer, COMP_TIMER);
      life->value = 5.0f;

      break;
    }
  }
}
