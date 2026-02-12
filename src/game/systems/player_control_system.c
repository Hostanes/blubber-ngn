#include "../../engine/util/bitset.h"
#include "../game.h"
#include "systems.h"
#include <raymath.h>

static Vector3 playerWeaponSway;
const float swayAmount = .05f;
const float swaySpeed = 15.0f;

const float speed = 15.0f;
const float mouseSensitivity = 0.002f;

const float jumpVelocity = 15.0f;
const float jumpCoyoteTimeMax = 0.3f;

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

  Position *playerPos = ECS_GET(world, player, Position, COMP_POSITION);
  Orientation *ori = ECS_GET(world, player, Orientation, COMP_ORIENTATION);
  ModelCollection_t *mc = ECS_GET(world, player, ModelCollection_t, COMP_MODEL);

  ModelInstance_t *gun = &mc->models[1];

  Vector3 forward = {
      cosf(ori->pitch) * sinf(ori->yaw),
      sinf(ori->pitch),
      cosf(ori->pitch) * cosf(ori->yaw),
  };

  Vector3 right = {
      cosf(ori->yaw),
      0.0f,
      -sinf(ori->yaw),
  };

  Vector3 up = {0.0f, 1.0f, 0.0f};

  // Start at player position
  Vector3 muzzlePos = playerPos->value;

  // Apply gun offset in world space
  muzzlePos = Vector3Add(muzzlePos, Vector3Scale(right, gun->offset.x));
  muzzlePos = Vector3Add(muzzlePos, Vector3Scale(up, gun->offset.y));
  muzzlePos = Vector3Add(muzzlePos, Vector3Scale(forward, gun->offset.z));

  // Push forward to barrel tip
  muzzlePos = Vector3Add(muzzlePos, Vector3Scale(forward, 0.4f));

  // --- Find inactive bullet ---
  archetype_t *bulletArch = WorldGetArchetype(world, game->bulletArchId);

  for (uint32_t i = 0; i < bulletArch->count; i++) {
    entity_t b = bulletArch->entities[i];

    Active *active = ECS_GET(world, b, Active, COMP_ACTIVE);
    if (active->value)
      continue;

    // --- Initialize bullet ---
    active->value = true;

    ECS_GET(world, b, Position, COMP_POSITION)->value = muzzlePos;
    ECS_GET(world, b, Velocity, COMP_VELOCITY)->value =
        Vector3Scale(forward, 220.0f);

    // Orientation
    Orientation *bori = ECS_GET(world, b, Orientation, COMP_ORIENTATION);
    bori->yaw = ori->yaw;
    bori->pitch = ori->pitch;

    // Model rotation = orientation
    ModelCollection_t *bmc = ECS_GET(world, b, ModelCollection_t, COMP_MODEL);
    bmc->models[0].rotation = (Vector3){-bori->pitch, 0.0f, 0.0f};

    ECS_GET(world, b, BulletType, COMP_BULLETTYPE)->type = 0;

    Timer *life = ECS_GET(world, b, Timer, COMP_TIMER);
    life->value = 5.0f; // seconds
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
