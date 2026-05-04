#include "../../engine/util/bitset.h"
#include "../game.h"
#include "systems.h"
#include <raymath.h>

static Vector3 playerWeaponSway;
const float swayAmount = .1f;
const float swaySpeed = 15.0f;

const float dashSpeed = 50.0f;
const float dashDuration = 0.18f;
const float dashCooldownMax = 0.9f;
const float speed = 20.0f;
const float mouseSensitivity = 0.002f;

const float jumpVelocity = 20.0f;
const float jumpCoyoteTimeMax = 0.3f;

static Vector3 GetForwardFromOrientation(const Orientation *ori) {
  return (Vector3){
      cosf(ori->pitch) * sinf(ori->yaw),
      sinf(ori->pitch),
      cosf(ori->pitch) * cosf(ori->yaw),
  };
}

void PlayerShootSystem(world_t *world, GameWorld *game, entity_t player,
                       float dt) {
  MuzzleCollection_t *muzzles =
      ECS_GET(world, player, MuzzleCollection_t, COMP_MUZZLES);

  if (!muzzles || muzzles->count == 0)
    return;

  int wi = (int)game->playerActiveWeapon;
  if (wi < 0 || wi >= muzzles->count)
    return;

  Muzzle_t *m = &muzzles->Muzzles[wi];

  if (m->fireTimer > 0.0f) {
    m->fireTimer -= dt;
    return;
  }

  bool trigger = (m->fireRate > 0.0f) ? IsMouseButtonDown(MOUSE_BUTTON_LEFT)
                                      : IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  if (!trigger)
    return;

  FireMuzzle(world, game, player, game->playerArchId, m);

  m->recoil += 0.15f;
  if (m->recoil > 0.25f) m->recoil = 0.25f;

  if (m->fireRate > 0.0f)
    m->fireTimer = 1.0f / m->fireRate;
}

void PlayerWeaponSwitchSystem(world_t *world, GameWorld *game,
                              entity_t player) {
  ModelCollection_t *mc = ECS_GET(world, player, ModelCollection_t, COMP_MODEL);

  if (!mc || mc->count <= 1)
    return;

  // index 0 = body, last index = shadow — weapons are indices 1..count-2
  uint32_t weaponCount = mc->count > 2 ? mc->count - 2 : 0;

  if (IsKeyPressed(KEY_ONE) && weaponCount >= 1)
    game->playerActiveWeapon = 0;

  if (IsKeyPressed(KEY_TWO) && weaponCount >= 2)
    game->playerActiveWeapon = 1;

  if (IsKeyPressed(KEY_THREE) && weaponCount >= 3)
    game->playerActiveWeapon = 2;

  // Clamp safety
  if (game->playerActiveWeapon >= weaponCount)
    game->playerActiveWeapon = 0;

  // Update model active states (index 0 = body, last = shadow — skip both)
  uint32_t gunEnd = mc->count > 1 ? mc->count - 1 : 1;
  for (uint32_t i = 1; i < gunEnd; ++i) {
    mc->models[i].isActive = ((i - 1) == game->playerActiveWeapon);
  }
}

void PlayerControlSystem(world_t *world, GameWorld *game, entity_t player,
                         float dt) {
  Position *pos = ECS_GET(world, player, Position, COMP_POSITION);
  Velocity *vel = ECS_GET(world, player, Velocity, COMP_VELOCITY);
  Orientation *ori = ECS_GET(world, player, Orientation, COMP_ORIENTATION);

  Timer *coyoteTimer = ECS_GET(world, player, Timer, COMP_COYOTETIMER);
  Timer *dashTimer = ECS_GET(world, player, Timer, COMP_DASHTIMER);
  Timer *dashCooldown = ECS_GET(world, player, Timer, COMP_DASHCOOLDOWN);

  bool *isgrounded = ECS_GET(world, game->player, bool, COMP_ISGROUNDED);
  bool *isDashing = ECS_GET(world, game->player, bool, COMP_ISDASHING);

  // DASH ACTIVE
  if (*isDashing) {
    if (dashTimer->value <= 0.0f) {
      *isDashing = false;
    }
    return;
  }

  // COYOTE TIME
  if (*isgrounded)
    coyoteTimer->value = jumpCoyoteTimeMax;
  else if (coyoteTimer->value < 0.0f)
    coyoteTimer->value = 0.0f;

  // CAMERA (only when NOT dashing)
  Vector2 mouse = GetMouseDelta();
  ori->yaw -= mouse.x * mouseSensitivity;
  ori->pitch -= mouse.y * mouseSensitivity;
  ori->pitch = Clamp(ori->pitch, -PI / 2 + 0.001f, PI / 2 - 0.001f);

  // -------------------------------------
  // MOVEMENT INPUT
  // -------------------------------------
  Vector3 forward = {
      cosf(ori->pitch) * sinf(ori->yaw),
      0,
      cosf(ori->pitch) * cosf(ori->yaw),
  };

  Vector3 right = {cosf(ori->yaw), 0, -sinf(ori->yaw)};

  float vy = vel->value.y;
  vel->value = (Vector3){0, vy, 0};

  if (IsKeyDown(KEY_W))
    vel->value = Vector3Add(vel->value, forward);
  if (IsKeyDown(KEY_S))
    vel->value = Vector3Subtract(vel->value, forward);
  if (IsKeyDown(KEY_A))
    vel->value = Vector3Add(vel->value, right);
  if (IsKeyDown(KEY_D))
    vel->value = Vector3Subtract(vel->value, right);

  // DASH TRIGGER
  if (IsKeyPressed(KEY_LEFT_SHIFT) && dashCooldown->value <= 0) {
    Vector3 horiz = {vel->value.x, 0.0f, vel->value.z};
    dashCooldown->value = dashCooldownMax;
    if (Vector3Length(horiz) > 0.01f) {
      horiz = Vector3Normalize(horiz);

      vel->value.x = horiz.x * dashSpeed;
      vel->value.z = horiz.z * dashSpeed;

      dashTimer->value = dashDuration;
      *isDashing = true;

      return; // skip rest of input
    }
  }

  // JUMP
  bool canJump = (coyoteTimer->value > 0.0f);

  if (IsKeyPressed(KEY_SPACE) && canJump) {
    vel->value.y = jumpVelocity;
    coyoteTimer->value = 0.0f;
  }

  // NORMAL MOVE SPEED
  Vector3 horiz = {vel->value.x, 0.0f, vel->value.z};

  if (Vector3Length(horiz) > 0.0f)
    horiz = Vector3Scale(Vector3Normalize(horiz), speed);

  vel->value.x = horiz.x;
  vel->value.z = horiz.z;
}

void PlayerWeaponSystem(world_t *world, GameWorld *game, entity_t player, float dt) {
  Orientation *ori = ECS_GET(world, player, Orientation, COMP_ORIENTATION);
  Velocity *vel = ECS_GET(world, player, Velocity, COMP_VELOCITY);
  Position *pos = ECS_GET(world, player, Position, COMP_POSITION);
  ModelCollection_t *mc = ECS_GET(world, player, ModelCollection_t, COMP_MODEL);

  if (!mc || mc->count <= 1)
    return;

  // Shadow (index 3): snap to terrain, slightly above to avoid clipping
  if (mc->count > 3) {
    float terrainY = HeightMap_GetHeightCatmullRom(
        &game->terrainHeightMap, pos->value.x, pos->value.z);
    mc->models[3].offset.y = terrainY - pos->value.y + 0.1f;
  }

  Vector3 forward = {sinf(ori->yaw), 0.0f, cosf(ori->yaw)};
  Vector3 right = {cosf(ori->yaw), 0.0f, -sinf(ori->yaw)};

  float lateral = Vector3DotProduct(vel->value, right);
  float longitudinal = Vector3DotProduct(vel->value, forward);
  float vertical = vel->value.y;

  float maxSpeed = 15.0f;

  float lateralNorm = Clamp(lateral / maxSpeed, -1.0f, 1.0f);
  float forwardNorm = Clamp(longitudinal / maxSpeed, -1.0f, 1.0f);
  float verticalNorm = Clamp(vertical / jumpVelocity, -1.0f, 1.0f);

  Vector3 targetSway = {-lateralNorm * swayAmount,
                        -fabsf(forwardNorm) * swayAmount * 0.6f -
                            verticalNorm * swayAmount * 1.2f,
                        0.0f};

  playerWeaponSway = Vector3Lerp(playerWeaponSway, targetSway, dt * swaySpeed);

  static float swayTime = 0.0f;
  swayTime += dt;

  Vector3 idleSway = {sinf(swayTime * 1.7f) * swayAmount * 0.08f,
                      cosf(swayTime * 2.3f) * swayAmount * 0.06f, 0.0f};

  Vector3 finalOffset = Vector3Add((Vector3){0, -0.25f, 0},
                                   Vector3Add(playerWeaponSway, idleSway));

  MuzzleCollection_t *muzzles =
      ECS_GET(world, player, MuzzleCollection_t, COMP_MUZZLES);

  if (!muzzles)
    return;

  // ---- Update gun models (skip index 0 = body, skip last = shadow) ----
  uint32_t gunEnd = mc->count > 3 ? mc->count - 1 : mc->count;
  for (uint32_t i = 1; i < gunEnd; ++i) {
    ModelInstance_t *gun = &mc->models[i];
    gun->rotation.x = -ori->pitch;

    // Recoil: muzzle index = model index - 1
    float recoilZ = 0.0f;
    uint32_t mi = i - 1;
    if (mi < (uint32_t)muzzles->count) {
      Muzzle_t *mz = &muzzles->Muzzles[mi];
      mz->recoil *= 1.0f - 14.0f * dt;
      if (mz->recoil < 0.001f) mz->recoil = 0.0f;
      recoilZ = -mz->recoil;
    }

    gun->offset = (Vector3){finalOffset.x, finalOffset.y, finalOffset.z + recoilZ};
  }

  // ---- Update all muzzles ----

  Vector3 forward3D = {cosf(ori->pitch) * sinf(ori->yaw), sinf(ori->pitch),
                       cosf(ori->pitch) * cosf(ori->yaw)};

  Vector3 right3D =
      Vector3Normalize(Vector3CrossProduct(forward3D, (Vector3){0, 1, 0}));

  Vector3 up3D = Vector3Normalize(Vector3CrossProduct(right3D, forward3D));

  for (uint32_t i = 0; i < muzzles->count; ++i) {
    Muzzle_t *m = &muzzles->Muzzles[i];

    Vector3 offset = m->positionOffset.value;

    Vector3 worldOffset =
        Vector3Add(Vector3Scale(right3D, offset.x),
                   Vector3Add(Vector3Scale(up3D, offset.y),
                              Vector3Scale(forward3D, offset.z)));

    m->worldPosition = Vector3Add(pos->value, worldOffset);

    m->forward = forward3D;
  }
}
