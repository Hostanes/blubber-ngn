#include "../../engine/util/bitset.h"
#include "../game.h"
#include "systems.h"
#include <raymath.h>

static Vector3 playerWeaponSway;
const float swayAmount = .05f;
const float swaySpeed = 15.0f;

const float dashSpeed = 50.0f;
const float dashDuration = 0.18f;
const float dashCooldownMax = 0.4f;
const float speed = 20.0f;
const float mouseSensitivity = 0.002f;

const float jumpVelocity = 20.0f;
const float jumpCoyoteTimeMax = 0.15f;

static Vector3 GetForwardFromOrientation(const Orientation *ori) {
  return (Vector3){
      cosf(ori->pitch) * sinf(ori->yaw),
      sinf(ori->pitch),
      cosf(ori->pitch) * cosf(ori->yaw),
  };
}

void PlayerShootSystem(world_t *world, GameWorld *game, entity_t player) {
  if (!IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    return;

  MuzzleCollection_t *muzzles =
      ECS_GET(world, player, MuzzleCollection_t, COMP_MUZZLES);

  if (!muzzles || muzzles->count == 0)
    return;

  Muzzle_t *m = &muzzles->Muzzles[0]; // single muzzle for now

  Vector3 muzzlePos = m->worldPosition;
  Vector3 forward = m->forward;

  archetype_t *bulletArch = WorldGetArchetype(world, game->bulletArchId);

  for (uint32_t i = 0; i < bulletArch->count; i++) {
    entity_t b = bulletArch->entities[i];

    Active *active = ECS_GET(world, b, Active, COMP_ACTIVE);

    if (active->value)
      continue;

    // --- Activate bullet ---
    active->value = true;

    BulletType *bt = ECS_GET(world, b, BulletType, COMP_BULLETTYPE);

    bt->type = m->bulletType;

    float muzzleVelocity = muzzleVelocities[bt->type];

    // --- Position ---
    ECS_GET(world, b, Position, COMP_POSITION)->value = muzzlePos;

    // --- Velocity ---
    ECS_GET(world, b, Velocity, COMP_VELOCITY)->value =
        Vector3Scale(forward, muzzleVelocity);

    // --- Orientation ---
    Orientation *bori = ECS_GET(world, b, Orientation, COMP_ORIENTATION);

    BulletOwner *owner = ECS_GET(world, b, BulletOwner, COMP_BULLET_OWNER);

    owner->eId = player.id;
    owner->archId = game->playerArchId;

    // derive yaw/pitch from forward if needed
    bori->yaw = atan2f(forward.x, forward.z);
    bori->pitch = asinf(forward.y);

    // --- Model rotation ---
    ModelCollection_t *bmc = ECS_GET(world, b, ModelCollection_t, COMP_MODEL);

    bmc->models[0].rotation = (Vector3){-bori->pitch, 0.0f, 0.0f};

    // --- Lifetime ---
    Timer *life = ECS_GET(world, b, Timer, COMP_TIMER);

    life->value = 5.0f;

    printf("spawned bullet\n");
    break;
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

void PlayerWeaponSystem(world_t *world, entity_t player, float dt) {
  Orientation *ori = ECS_GET(world, player, Orientation, COMP_ORIENTATION);
  Velocity *vel = ECS_GET(world, player, Velocity, COMP_VELOCITY);
  ModelCollection_t *mc = ECS_GET(world, player, ModelCollection_t, COMP_MODEL);
  ModelInstance_t *gun = &mc->models[1];

  gun->rotation.x = -ori->pitch;

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

  // Idle motion
  static float swayTime = 0.0f;
  swayTime += dt;

  Vector3 idleSway = {sinf(swayTime * 1.7f) * swayAmount * 0.08f,
                      cosf(swayTime * 2.3f) * swayAmount * 0.06f, 0.0f};
  gun->offset = Vector3Add((Vector3){0, -0.25f, 0},
                           Vector3Add(playerWeaponSway, idleSway));

  MuzzleCollection_t *muzzles =
      ECS_GET(world, player, MuzzleCollection_t, COMP_MUZZLES);

  Position *playerPos = ECS_GET(world, player, Position, COMP_POSITION);

  Vector3 forward3D = {cosf(ori->pitch) * sinf(ori->yaw), sinf(ori->pitch),
                       cosf(ori->pitch) * cosf(ori->yaw)};

  Vector3 right3D =
      Vector3Normalize(Vector3CrossProduct(forward3D, (Vector3){0, 1, 0}));

  Vector3 up3D = Vector3Normalize(Vector3CrossProduct(right3D, forward3D));

  for (int i = 0; i < muzzles->count; i++) {
    Muzzle_t *m = &muzzles->Muzzles[i];

    Vector3 offset = m->positionOffset.value;

    Vector3 worldOffset =
        Vector3Add(Vector3Scale(right3D, offset.x),
                   Vector3Add(Vector3Scale(up3D, offset.y),
                              Vector3Scale(forward3D, offset.z)));

    m->worldPosition = Vector3Add(playerPos->value, worldOffset);
    m->forward = forward3D;
  }
}
