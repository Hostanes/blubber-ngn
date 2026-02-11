#include "systems.h"

static Vector3 playerWeaponSway;
const float swayAmount = 0.15f; // meters
const float swaySpeed = 15.0f;  // responsiveness

void PlayerControlSystem(world_t *world, entity_t player) {
  Position *pos = ECS_GET(world, player, Position, COMP_POSITION);
  Velocity *vel = ECS_GET(world, player, Velocity, COMP_VELOCITY);
  Orientation *ori = ECS_GET(world, player, Orientation, COMP_ORIENTATION);

  (void)pos; // currently unused

  const float speed = 15.0f;
  const float mouseSensitivity = 0.002f;

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

  vel->value = (Vector3){0};

  if (IsKeyDown(KEY_W))
    vel->value = Vector3Add(vel->value, forward);
  if (IsKeyDown(KEY_S))
    vel->value = Vector3Subtract(vel->value, forward);
  if (IsKeyDown(KEY_A))
    vel->value = Vector3Add(vel->value, right);
  if (IsKeyDown(KEY_D))
    vel->value = Vector3Subtract(vel->value, right);

  if (Vector3Length(vel->value) > 0.0f) {
    vel->value = Vector3Scale(Vector3Normalize(vel->value), speed);
  }
}

void PlayerWeaponSystem(world_t *world, entity_t player, float dt) {
  Orientation *ori = ECS_GET(world, player, Orientation, COMP_ORIENTATION);
  Velocity *vel = ECS_GET(world, player, Velocity, COMP_VELOCITY);
  ModelCollection_t *mc = ECS_GET(world, player, ModelCollection_t, COMP_MODEL);

  ModelInstance_t *gun = &mc->models[1];

  // Base rotation from pitch
  gun->rotation.x = -ori->pitch;

  // Camera basis
  Vector3 forward = {
      sinf(ori->yaw),
      0.0f,
      cosf(ori->yaw),
  };

  Vector3 right = {
      cosf(ori->yaw),
      0.0f,
      -sinf(ori->yaw),
  };

  // Project velocity into camera space
  float lateral = Vector3DotProduct(vel->value, right);
  float longitudinal = Vector3DotProduct(vel->value, forward);

  // Target sway (invert so motion feels weighted)
  Vector3 targetSway = {-lateral * 0.002f,             // left/right
                        -fabsf(longitudinal) * 0.001f, // slight downward bob
                        0.0f};

  // Clamp
  targetSway.x = Clamp(targetSway.x, -0.04f, 0.04f);
  targetSway.y = Clamp(targetSway.y, -0.03f, 0.0f);

  // Smoothly approach target (critical for feel)
  playerWeaponSway = Vector3Lerp(playerWeaponSway, targetSway, dt * swaySpeed);

  // Apply sway to model offset
  gun->offset = Vector3Add((Vector3){0, -0.9f, 0}, // base offset
                           playerWeaponSway);
}
