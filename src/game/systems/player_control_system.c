#include "../game.h"
#include "systems.h"

static Vector3 playerWeaponSway;
const float swayAmount = .05f;
const float swaySpeed = 15.0f;

const float speed = 20.0f;
const float mouseSensitivity = 0.002f;

const float jumpVelocity = 10.0f;
const float jumpCoyoteTimeMax = 0.3f;

void PlayerControlSystem(world_t *world, GameWorld *game, entity_t player,
                         float dt) {
  Position *pos = ECS_GET(world, player, Position, COMP_POSITION);
  Velocity *vel = ECS_GET(world, player, Velocity, COMP_VELOCITY);
  Orientation *ori = ECS_GET(world, player, Orientation, COMP_ORIENTATION);

  Timer *coyoteTimer = ECS_GET(world, player, Timer, COMP_COYOTETIMER);

  float groundY = HeightMap_GetHeightSmooth(&game->terrainHeightMap,
                                            pos->value.x, pos->value.z);
  float footY = pos->value.y - 2.0f;
  bool isOnGround = (footY <= groundY + 0.02f);

  if (isOnGround) {
    // Refresh coyote time while grounded
    coyoteTimer->value = jumpCoyoteTimeMax;
  } else {
    if (coyoteTimer->value < 0.0f)
      coyoteTimer->value = 0.0f;
  }

  Vector2 mouse = GetMouseDelta();
  ori->yaw -= mouse.x * mouseSensitivity;
  ori->pitch -= mouse.y * mouseSensitivity;

  ori->pitch = Clamp(ori->pitch, -PI / 2 + 0.001, PI / 2 - 0.001);

  Vector3 forward = {
      cosf(ori->pitch) * sinf(ori->yaw),
      // sinf(ori->pitch),
      0,
      cosf(ori->pitch) * cosf(ori->yaw),
  };
  Vector3 right = {cosf(ori->yaw), 0.0f, -sinf(ori->yaw)};
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

  bool canJump = (coyoteTimer->value > 0.0f);

  if (IsKeyPressed(KEY_SPACE) && canJump) {
    vel->value.y = jumpVelocity;

    // Consume coyote time immediately
    coyoteTimer->value = 0.0f;
  }

  Vector3 horiz = {vel->value.x, 0.0f, vel->value.z};

  if (Vector3Length(horiz) > 0.0f) {
    horiz = Vector3Scale(Vector3Normalize(horiz), speed);
  }

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

  gun->offset = Vector3Add((Vector3){0, -0.9f, 0},
                           Vector3Add(playerWeaponSway, idleSway));
}
