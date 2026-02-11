#include "systems.h"

void PlayerControlSystem(world_t *world, entity_t player) {
  Position *pos = ECS_GET(world, player, Position, COMP_POSITION);
  Velocity *vel = ECS_GET(world, player, Velocity, COMP_VELOCITY);
  Orientation *ori = ECS_GET(world, player, Orientation, COMP_ORIENTATION);

  (void)pos; // currently unused

  const float speed = 5.0f;
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

void PlayerWeaponSystem(world_t *world, entity_t player) {
  Orientation *ori = ECS_GET(world, player, Orientation, COMP_ORIENTATION);
  ModelCollection_t *mc = ECS_GET(world, player, ModelCollection_t, COMP_MODEL);

  ModelInstance_t *gun = &mc->models[1]; // temporary, fine for now

  gun->rotation.x = -ori->pitch;
}
