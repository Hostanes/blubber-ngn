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

static void BuildBasisFromOrientation(Orientation *ori, Vector3 *forward,
                                      Vector3 *right, Vector3 *up) {
  *forward = (Vector3){cosf(ori->pitch) * sinf(ori->yaw), sinf(ori->pitch),
                       cosf(ori->pitch) * cosf(ori->yaw)};

  *right = Vector3Normalize(Vector3CrossProduct(*forward, (Vector3){0, 1, 0}));

  *up = Vector3Normalize(Vector3CrossProduct(*right, *forward));
}

void EnemyMuzzleUpdate_Grunt(world_t *world, archetype_t *arch) {
  for (uint32_t i = 0; i < arch->count; i++) {
    entity_t e = arch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value)
      continue;

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);

    Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);

    MuzzleCollection_t *muzzles =
        ECS_GET(world, e, MuzzleCollection_t, COMP_MUZZLES);

    if (!pos || !ori || !muzzles || muzzles->count == 0)
      continue;

    Vector3 forward, right, up;
    BuildBasisFromOrientation(ori, &forward, &right, &up);

    Muzzle_t *m = &muzzles->Muzzles[0];

    Vector3 offset = m->positionOffset.value;

    Vector3 worldOffset =
        Vector3Add(Vector3Scale(right, offset.x),
                   Vector3Add(Vector3Scale(up, offset.y),
                              Vector3Scale(forward, offset.z)));

    m->worldPosition = Vector3Add(pos->value, worldOffset);

    // Force vertical firing
    m->forward = (Vector3){0, 1, 0};
  }
}

void EnemyMuzzleUpdate_Missile(world_t *world, archetype_t *arch) {
  for (uint32_t i = 0; i < arch->count; i++) {
    entity_t e = arch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value)
      continue;

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);

    MuzzleCollection_t *muzzles =
        ECS_GET(world, e, MuzzleCollection_t, COMP_MUZZLES);

    if (!pos || !muzzles || muzzles->count == 0)
      continue;

    Muzzle_t *m = &muzzles->Muzzles[0];

    m->worldPosition = Vector3Add(pos->value, (Vector3){0, 1.5f, 0});

    m->forward = (Vector3){0, 1, 0}; // straight up
  }
}
