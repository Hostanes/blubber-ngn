#include "../../engine/util/bitset.h"
#include "../game.h"
#include "../level_creater_helper.h"
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

  // Weapons 2 and 3 are handled by their own systems
  if (wi == 2 || wi == 3) return;

  Muzzle_t *m = &muzzles->Muzzles[wi];

  if (m->fireTimer > 0.0f) {
    m->fireTimer -= dt;
    return;
  }

  if (m->isOverheated)
    return;

  bool trigger = (m->fireRate > 0.0f) ? IsMouseButtonDown(MOUSE_BUTTON_LEFT)
                                      : IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
  if (!trigger)
    return;

  FireMuzzle(world, game, player, game->playerArchId, m);

  float fireVol = (wi == 1) ? 0.45f : 0.45f;
  float firePitch;
  switch (wi) {
  case 0:
    firePitch = 0.8;
    break;
  case 1:
    firePitch = 1.5;
    break;
  case 2:
    firePitch = 0.5;
  default:
    firePitch = 1.0f;
    break;
  }
  QueueSound(&game->soundSystem, SOUND_WEAPON_FIRE, m->worldPosition, fireVol,
             firePitch);

  m->recoil += 0.15f;
  if (m->recoil > 0.25f)
    m->recoil = 0.25f;

  // Gunshot particles — 2 small puffs that drift upward with jitter
  for (int p = 0; p < 2; p++) {
    float jx = ((float)GetRandomValue(-80, 80)) / 100.0f;
    float jz = ((float)GetRandomValue(-80, 80)) / 100.0f;
    Vector3 pvel = {jx * 0.4f, 0.6f + ((float)GetRandomValue(20, 60)) / 100.0f,
                    jz * 0.4f};
    Color pCol = {255, 200, 80, 200};
    SpawnParticle(world, game, m->worldPosition, pvel, 0.06f, 0.35f, pCol);
  }

  m->heat += m->heatPerShot;
  if (m->heat >= 1.0f) {
    m->heat = 1.0f;
    m->isOverheated = true;
  }
  m->coolDelayTimer = m->coolDelay;

  if (m->fireRate > 0.0f)
    m->fireTimer = 1.0f / m->fireRate;
}

void PlayerWeaponSwitchSystem(world_t *world, GameWorld *game,
                              entity_t player) {
  ModelCollection_t *mc = ECS_GET(world, player, ModelCollection_t, COMP_MODEL);

  if (!mc || mc->count <= 1)
    return;

  // index 0 = body, weapons are indices 1..count-1
  uint32_t weaponCount = mc->count > 1 ? mc->count - 1 : 0;

  if (IsKeyPressed(KEY_ONE) && weaponCount >= 1)
    game->playerActiveWeapon = 0;

  if (IsKeyPressed(KEY_TWO) && weaponCount >= 2)
    game->playerActiveWeapon = 1;

  if (IsKeyPressed(KEY_THREE) && weaponCount >= 3)
    game->playerActiveWeapon = 2;

  if (IsKeyPressed(KEY_FOUR) && weaponCount >= 4)
    game->playerActiveWeapon = 3;

  // Clamp safety
  if (game->playerActiveWeapon >= weaponCount)
    game->playerActiveWeapon = 0;

  // Update model active states (index 0 = body, guns are 1..count-1)
  for (uint32_t i = 1; i < mc->count; ++i) {
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

  // Footstep sound — play while grounded and moving
  static float stepTimer = 0.0f;
  stepTimer -= dt;
  float horizSpeed =
      sqrtf(vel->value.x * vel->value.x + vel->value.z * vel->value.z);
  if (*isgrounded && horizSpeed > 1.0f && stepTimer <= 0.0f) {
    float interval = 0.45f - 0.15f * Clamp(horizSpeed / 15.0f, 0.0f, 1.0f);
    QueueSound(&game->soundSystem, SOUND_FOOTSTEP, pos->value, 0.1f,
               0.9f + GetRandomValue(-10, 10) / 100.0f);
    stepTimer = interval;
  }
}

void PlayerWeaponSystem(world_t *world, GameWorld *game, entity_t player,
                        float dt) {
  Orientation *ori = ECS_GET(world, player, Orientation, COMP_ORIENTATION);
  Velocity *vel = ECS_GET(world, player, Velocity, COMP_VELOCITY);
  Position *pos = ECS_GET(world, player, Position, COMP_POSITION);
  ModelCollection_t *mc = ECS_GET(world, player, ModelCollection_t, COMP_MODEL);

  if (!mc || mc->count <= 1)
    return;


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

  // ---- Update gun models (index 0 = body, skip; guns are 1..count-1) ----
  for (uint32_t i = 1; i < mc->count; ++i) {
    ModelInstance_t *gun = &mc->models[i];
    gun->rotation.x = -ori->pitch;

    // Recoil: muzzle index = model index - 1
    float recoilZ = 0.0f;
    uint32_t mi = i - 1;
    if (mi < (uint32_t)muzzles->count) {
      Muzzle_t *mz = &muzzles->Muzzles[mi];
      mz->recoil *= 1.0f - 14.0f * dt;
      if (mz->recoil < 0.001f)
        mz->recoil = 0.0f;
      recoilZ = -mz->recoil;
    }

    gun->offset =
        (Vector3){finalOffset.x, finalOffset.y, finalOffset.z + recoilZ};
  }

  // ---- Update all muzzles ----

  Vector3 forward3D = {cosf(ori->pitch) * sinf(ori->yaw), sinf(ori->pitch),
                       cosf(ori->pitch) * cosf(ori->yaw)};

  Vector3 right3D =
      Vector3Normalize(Vector3CrossProduct(forward3D, (Vector3){0, 1, 0}));

  Vector3 up3D = Vector3Normalize(Vector3CrossProduct(right3D, forward3D));

  for (uint32_t i = 0; i < muzzles->count; ++i) {
    Muzzle_t *m = &muzzles->Muzzles[i];

    // Cool heat every frame regardless of which weapon is active,
    // but only after the post-fire delay has elapsed
    if (m->coolDelayTimer > 0.0f) {
      m->coolDelayTimer -= dt;
    } else {
      float rate = m->isOverheated ? m->coolRateOverheated : m->coolRate;
      m->heat -= rate * dt;
      if (m->heat < 0.0f)
        m->heat = 0.0f;
      if (m->isOverheated && m->heat <= m->overheatThreshold)
        m->isOverheated = false;
    }

    Vector3 offset = m->positionOffset.value;

    Vector3 worldOffset =
        Vector3Add(Vector3Scale(right3D, offset.x),
                   Vector3Add(Vector3Scale(up3D, offset.y),
                              Vector3Scale(forward3D, offset.z)));

    m->worldPosition = Vector3Add(pos->value, worldOffset);

    m->forward = forward3D;

    // Tint the corresponding gun model redder as heat rises.
    uint32_t modelIdx = i + 1;
    if (modelIdx < mc->count) {
      float t = m->heat;
      unsigned char g = (unsigned char)(255.0f * (1.0f - t * 0.65f));
      unsigned char b = (unsigned char)(255.0f * (1.0f - t * 0.65f));
      mc->models[modelIdx].tint = (Color){255, g, b, 255};
    }

    // Smoke particles while overheated and this weapon is equipped
    if (m->isOverheated && i == game->playerActiveWeapon) {
      m->smokeTimer -= dt;
      if (m->smokeTimer <= 0.0f) {
        m->smokeTimer = 0.06f;
        float rx = ((float)GetRandomValue(-60, 60)) / 100.0f;
        float rz = ((float)GetRandomValue(-60, 60)) / 100.0f;
        Vector3 smokePos = {m->worldPosition.x + rx * 0.3f,
                            m->worldPosition.y + 0.1f,
                            m->worldPosition.z + rz * 0.3f};
        Vector3 smokeVel = {rx * 0.3f,
                            0.4f + ((float)GetRandomValue(0, 40)) / 100.0f,
                            rz * 0.3f};
        Color smokeCol = {80, 80, 80, 180};
        SpawnParticle(world, game, smokePos, smokeVel, 0.12f, 0.9f, smokeCol);
      }
    } else {
      m->smokeTimer = 0.0f;
    }
  }
}

// ---------------------------------------------------------------------------
// Rocket launcher — lock-on weapon (weapon slot 2)
// ---------------------------------------------------------------------------

#define ROCKET_LOCK_TIME      1.2f
#define ROCKET_LOCK_RADIUS    180.0f  // pixels from screen center
#define ROCKET_BURST_COUNT    3
#define ROCKET_BURST_INTERVAL 0.15f   // seconds between missiles
#define ROCKET_MAX_TARGETS    3

void RocketLauncherSystem(world_t *world, GameWorld *game,
                          entity_t player, Camera3D *camera, float dt) {

  MuzzleCollection_t *mc = ECS_GET(world, player, MuzzleCollection_t, COMP_MUZZLES);
  if (!mc || mc->count < 3) return;
  Muzzle_t *m = &mc->Muzzles[2];

  // --- Burst in progress: fire missiles one at a time ---
  if (game->rocketLockState == LOCKSTATE_BURSTING) {
    game->rocketLockAngle += 360.0f * dt;
    game->rocketBurstTimer -= dt;
    if (game->rocketBurstTimer <= 0.0f) {
      int n = game->rocketLockTargetCount;
      if (game->rocketBurstGuided && n > 0) {
        entity_t tgt = game->rocketLockTargets[game->rocketBurstIndex % n];
        SpawnHomingMissile(world, game, player, tgt,
                           m->worldPosition, m->forward, true, 100.0f);
      } else {
        float yawOff = ((float)game->rocketBurstIndex - (ROCKET_BURST_COUNT - 1) * 0.5f) * 0.06f;
        float cy = cosf(yawOff), sy = sinf(yawOff);
        Vector3 fwd = {
          m->forward.x * cy + m->forward.z * sy,
          m->forward.y,
          -m->forward.x * sy + m->forward.z * cy,
        };
        SpawnHomingMissile(world, game, player, (entity_t){0},
                           m->worldPosition, fwd, false, 0.0f);
      }
      m->heat += m->heatPerShot;
      if (m->heat >= 1.0f) { m->heat = 1.0f; m->isOverheated = true; }
      m->coolDelayTimer = m->coolDelay;
      QueueSound(&game->soundSystem, SOUND_ROCKET_FIRE, m->worldPosition, 0.9f, 1.0f);
      game->rocketBurstIndex++;
      game->rocketBurstRemaining--;
      if (game->rocketBurstRemaining <= 0) {
        game->rocketLockState       = LOCKSTATE_IDLE;
        game->rocketLockProgress    = 0.0f;
        game->rocketLockTargetCount = 0;
      } else {
        game->rocketBurstTimer = ROCKET_BURST_INTERVAL;
      }
    }
    return;
  }

  if (game->playerActiveWeapon != 2) {
    game->rocketLockState       = LOCKSTATE_IDLE;
    game->rocketLockProgress    = 0.0f;
    game->rocketLockTargetCount = 0;
    return;
  }

  bool lmbHeld     = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
  bool lmbReleased = IsMouseButtonReleased(MOUSE_BUTTON_LEFT);
  bool rmbPressed  = IsMouseButtonPressed(MOUSE_BUTTON_RIGHT);

  // Cancel: RMB while charging
  if (rmbPressed && lmbHeld) {
    game->rocketLockState       = LOCKSTATE_IDLE;
    game->rocketLockProgress    = 0.0f;
    game->rocketLockTargetCount = 0;
    return;
  }

  // Start burst on LMB release (blocked while overheated)
  if (lmbReleased && game->rocketLockState != LOCKSTATE_IDLE) {
    if (m->isOverheated) {
      game->rocketLockState    = LOCKSTATE_IDLE;
      game->rocketLockProgress = 0.0f;
      return;
    }
    game->rocketBurstGuided    = (game->rocketLockState == LOCKSTATE_LOCKED);
    game->rocketLockState      = LOCKSTATE_BURSTING;
    game->rocketBurstRemaining = ROCKET_BURST_COUNT;
    game->rocketBurstIndex     = 0;
    game->rocketBurstTimer     = 0.0f;
    return;
  }

  if (!lmbHeld || m->isOverheated) return;

  if (game->rocketLockState == LOCKSTATE_IDLE)
    game->rocketLockState = LOCKSTATE_ACQUIRING;

  float spinRate = (game->rocketLockState == LOCKSTATE_LOCKED) ? 360.0f : 180.0f;
  game->rocketLockAngle += spinRate * dt;

  // Scan ALL enemies in reticle, collect up to ROCKET_MAX_TARGETS
  int sw = GetScreenWidth(), sh = GetScreenHeight();
  Vector2 center = {(float)(sw / 2), (float)(sh / 2)};

  uint32_t archIds[] = {
    game->enemyGruntArchId,
    game->enemyRangerArchId,
    game->enemyMeleeArchId,
    game->targetStaticArchId,
    game->targetPatrolArchId,
  };

  game->rocketLockTargetCount = 0;

  for (int ai = 0; ai < 5; ai++) {
    archetype_t *arch = WorldGetArchetype(world, archIds[ai]);
    if (!arch) continue;
    for (uint32_t i = 0; i < arch->count; i++) {
      if (game->rocketLockTargetCount >= ROCKET_MAX_TARGETS) break;
      entity_t e = arch->entities[i];
      Active *act = ECS_GET(world, e, Active, COMP_ACTIVE);
      if (!act || !act->value) continue;
      Position *epos = ECS_GET(world, e, Position, COMP_POSITION);
      if (!epos) continue;
      Vector3 camFwd  = Vector3Normalize(Vector3Subtract(camera->target, camera->position));
      Vector3 toEnemy = Vector3Subtract(epos->value, camera->position);
      if (Vector3DotProduct(camFwd, toEnemy) <= 0.0f) continue;
      Vector2 screen = GetWorldToScreen(epos->value, *camera);
      if (Vector2Distance(screen, center) <= ROCKET_LOCK_RADIUS)
        game->rocketLockTargets[game->rocketLockTargetCount++] = e;
    }
  }

  if (game->rocketLockTargetCount > 0) {
    game->rocketLockProgress += dt / ROCKET_LOCK_TIME;
    if (game->rocketLockProgress >= 1.0f) {
      game->rocketLockProgress = 1.0f;
      game->rocketLockState    = LOCKSTATE_LOCKED;
    }
  } else {
    game->rocketLockProgress -= dt * 2.0f;
    if (game->rocketLockProgress < 0.0f) game->rocketLockProgress = 0.0f;
    if (game->rocketLockState == LOCKSTATE_LOCKED)
      game->rocketLockState = LOCKSTATE_ACQUIRING;
  }
}
