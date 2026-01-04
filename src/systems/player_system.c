
#include "systems.h"
#include <math.h>

#define BOB_AMOUNT 0.5f

// -----------------------------
// Tunables
// -----------------------------
const float DASH_CHARGE_TIME = 0.18f;
const float DASH_GO_TIME = 0.12f;
const float DASH_SLOW_TIME = 0.10f;

const float DASH_SPEED = 1200.0f;
const float DASH_SLOW_DAMP = 18.0f;

// Torso pitch kick during dash (radians)
const float DASH_TORSO_KICK = 0.01f;

// FOV bump during dash
const float DASH_FOV_MULT = 1.06f;
const float FOV_SPEED = 12.0f;

// Torso kick smoothing (bigger = snappier)
const float KICK_EASE_IN = 30.0f;
const float KICK_EASE_OUT = 22.0f;

static float HEAT_MAX = 100.0f;
static float HEAT_COOL_PER_SEC = 15.0f; // heat/sec cooldown (tweak)

// Heat costs (tweak)
static float HEAT_COST_DASH = 25.0f;   // on dash start
static float HEAT_COST_LMB = 5.0f;     // per shot
static float HEAT_COST_RMB = 12.0f;    // per shot
static float HEAT_COST_ROCKET = 18.0f; // per rocket

extern Vector3 recoilOffset;

static inline float HeatClamp(float h) {
  if (h < 0.0f)
    return 0.0f;
  if (h > 100.0f)
    return 100.0f;
  return h;
}

static inline float HeatGet(const GameState_t *gs) {
  return (float)gs->heatMeter;
}

static inline void HeatSet(GameState_t *gs, float heat) {
  gs->heatMeter = (HeatClamp(heat));
}

// returns true if allowed (wont exceed 100)
static inline bool HeatCanSpend(const GameState_t *gs, float cost) {
  return (HeatGet(gs) + cost) <= 100.0f;
}

// adds heat (clamped)
static inline void HeatSpend(GameState_t *gs, float cost) {
  HeatSet(gs, HeatGet(gs) + cost);
}

// cools heat over time (clamped)
static inline void HeatCool(GameState_t *gs, float dt, float coolPerSec) {
  HeatSet(gs, HeatGet(gs) - coolPerSec * dt);
  // printf("cooled mech, heat at %f\n", HeatGet(gs));
}

// Try to perform an action that costs heat. If not enough budget, action is
// blocked. Returns true if action is allowed (heat spent), false if blocked.
static inline bool HeatTryAction(GameState_t *gs, float cost) {
  if (!HeatCanSpend(gs, cost))
    return false;
  HeatSpend(gs, cost);
  return true;
}

// Same, but for continuous costs (per second). Returns true if allowed this
// frame.
static inline bool HeatTryActionPerSec(GameState_t *gs, float dt,
                                       float costPerSec) {
  float cost = costPerSec * dt;
  return HeatTryAction(gs, cost);
}

// ========== GUN ==========

void UpdateRayDistance(GameState_t *gs, Engine_t *eng, entity_t e, float dt) {
  if (e < 0 || e >= eng->em.count)
    return;

  int rayIndex = 0; // the main torso ray
  Raycast_t *rc = &eng->actors.raycasts[e][rayIndex];

  // Get mouse wheel movement
  int wheelMove = GetMouseWheelMove(); // +1 scroll up, -1 scroll down
  if (wheelMove != 0) {
    float sensitivity = 50.0f; // units per wheel step
    rc->distance += wheelMove * sensitivity;

    // Clamp distance
    if (rc->distance < 100.0f)
      rc->distance = 100.0f;
    if (rc->distance > 2500.0f)
      rc->distance = 2500.0f;

    printf("Ray distance: %.2f\n", rc->distance);
  }
}

void ApplyTorsoRecoil(ModelCollection_t *mc, int torsoIndex, float intensity,
                      Vector3 direction) {
  if (Vector3Length(direction) > 0.0001f)
    direction = Vector3Normalize(direction);

  Orientation *torso = &mc->orientations[torsoIndex];

  // Add randomness
  float randYaw = ((float)rand() / RAND_MAX * 2.0f - 1.0f);
  float randPitch = ((float)rand() / RAND_MAX * 2.0f - 1.0f);

  float recoilYaw = (direction.x + randYaw * 0.3f) * intensity;
  float recoilPitch = (direction.y + randPitch * 0.3f) * intensity / 1.5;

  torso->yaw += recoilYaw;
  torso->pitch += recoilPitch;

  // Optional clamp (prevent over-rotation)
  if (torso->pitch > PI / 3.0f)
    torso->pitch = PI / 3.0f;
  if (torso->pitch < -PI / 3.0f)
    torso->pitch = -PI / 3.0f;
}

void UpdateTorsoRecoil(ModelCollection_t *mc, int torsoIndex, float dt) {
  Orientation *torso = &mc->orientations[torsoIndex];

  float stiffness = 8.0f;
  float damping = 6.0f;
}

static Vector3 ForwardFromLegYaw(float yaw) {
  // Matches your movement basis (forward = {cos(yaw),0,sin(yaw)})
  return (Vector3){cosf(yaw), 0.0f, sinf(yaw)};
}

void PlayerControlSystem(GameState_t *gs, Engine_t *eng,
                         SoundSystem_t *soundSys, float dt, Camera3D *camera) {
  int pid = gs->playerId;

  // -----------------------------
  // Component access / validation
  // -----------------------------
  Vector3 *pos =
      (Vector3 *)GetComponentArray(&eng->actors, gs->compReg.cid_Positions);
  Vector3 *vel =
      (Vector3 *)GetComponentArray(&eng->actors, gs->compReg.cid_velocities);

  int *pState =
      (int *)getComponent(&eng->actors, pid, gs->compReg.cid_moveBehaviour);
  float *pTimer =
      (float *)getComponent(&eng->actors, pid, gs->compReg.cid_moveTimer);

  if (!pos || !vel || !pState || !pTimer)
    return;

  ModelCollection_t *mc = &eng->actors.modelCollections[pid];
  if (!mc->orientations || mc->countModels < 2)
    return;

  Orientation *leg = &mc->orientations[0];
  Orientation *torso = &mc->orientations[1];

  bool isSprinting = IsKeyDown(KEY_LEFT_SHIFT);

  // -----------------------------
  // State helpers
  // -----------------------------
  bool controlsLocked = (*pState != PSTATE_NORMAL);

  // -----------------------------
  // DASH INPUT (uses heat)
  // -----------------------------
  if (IsKeyPressed(KEY_SPACE) && *pState == PSTATE_NORMAL) {
    if (HeatTryAction(gs, HEAT_COST_DASH)) {
      *pState = PSTATE_DASH_CHARGE;
      *pTimer = DASH_CHARGE_TIME;
      controlsLocked = true;
    }
  }

  // -----------------------------
  // DASH STATE MACHINE (movement locked)
  // -----------------------------
  switch ((PlayerMoveState)(*pState)) {
  case PSTATE_NORMAL:
    break;

  case PSTATE_DASH_CHARGE: {
    *pTimer -= dt;

    // soften existing motion while charging
    vel[pid].x *= 0.92f;
    vel[pid].z *= 0.92f;

    if (*pTimer <= 0.0f) {
      *pState = PSTATE_DASH_GO;
      *pTimer = DASH_GO_TIME;

      Vector3 fwd = Vector3Normalize(ForwardFromLegYaw(leg->yaw));
      vel[pid].x = fwd.x * DASH_SPEED;
      vel[pid].z = fwd.z * DASH_SPEED;
    }
  } break;

  case PSTATE_DASH_GO: {
    *pTimer -= dt;

    Vector3 fwd = Vector3Normalize(ForwardFromLegYaw(leg->yaw));
    vel[pid].x = fwd.x * DASH_SPEED;
    vel[pid].z = fwd.z * DASH_SPEED;

    if (*pTimer <= 0.0f) {
      *pState = PSTATE_DASH_SLOW;
      *pTimer = DASH_SLOW_TIME;
    }
  } break;

  case PSTATE_DASH_SLOW: {
    *pTimer -= dt;

    float damp = expf(-DASH_SLOW_DAMP * dt);
    vel[pid].x *= damp;
    vel[pid].z *= damp;

    if (*pTimer <= 0.0f) {
      *pState = PSTATE_NORMAL;
      *pTimer = 0.0f;
    }
  } break;
  }

  // Recompute lock after state machine
  controlsLocked = (*pState != PSTATE_NORMAL);

  // -----------------------------
  // Heat cooling (only when NOT dashing)
  // -----------------------------
  if (!controlsLocked) {
    HeatCool(gs, dt, HEAT_COOL_PER_SEC);
  }

  // -----------------------------
  // Camera / look controls
  // -----------------------------
  if (IsKeyPressed(KEY_B)) {
    gs->isZooming = !gs->isZooming;
  }
  float sensitivity = 0.0007f;
  if (gs->isZooming)
    sensitivity = 0.0002f;

  float turnRate = isSprinting ? 0.2f : 1.0f;

  if (!controlsLocked) {
    // Leg yaw with A/D
    if (IsKeyDown(KEY_A))
      leg->yaw -= 1.5f * dt * turnRate;
    if (IsKeyDown(KEY_D))
      leg->yaw += 1.5f * dt * turnRate;

    // Torso yaw/pitch with mouse
    Vector2 mouse = GetMouseDelta();
    torso->yaw += mouse.x * sensitivity;
    torso->pitch += -mouse.y * sensitivity;
  }

  UpdateRayDistance(gs, eng, gs->playerId, dt);

  // -----------------------------
  // Dash torso pitch kick (visual)
  // -----------------------------
  static float kickBlend = 0.0f;
  float targetBlend = 0.0f;

  if (*pState == PSTATE_DASH_CHARGE)
    targetBlend = 1.0f;
  else if (*pState == PSTATE_DASH_GO)
    targetBlend = 0.6f;
  else if (*pState == PSTATE_DASH_SLOW)
    targetBlend = 0.25f;
  else
    targetBlend = 0.0f;

  float k = (targetBlend > kickBlend) ? KICK_EASE_IN : KICK_EASE_OUT;
  kickBlend += (targetBlend - kickBlend) * (1.0f - expf(-k * dt));

  torso->pitch += DASH_TORSO_KICK * kickBlend;

  // Clamp torso pitch
  if (torso->pitch > 1.2f)
    torso->pitch = 1.2f;
  if (torso->pitch < -1.0f)
    torso->pitch = -1.0f;

  // -----------------------------
  // Movement basis from legs
  // -----------------------------
  float c = cosf(leg->yaw);
  float s = sinf(leg->yaw);
  Vector3 forward = (Vector3){c, 0, s};

  // -----------------------------
  // FOV
  // -----------------------------
  float baseFOV = eng->config.fov_deg;
  float sprintFOV = baseFOV * 1.1f;
  float targetFOV = isSprinting ? sprintFOV : baseFOV;

  if (controlsLocked) {
    gs->isZooming = false;
    targetFOV *= DASH_FOV_MULT;
  }
  if (gs->isZooming)
    targetFOV = 10.0f;

  camera->fovy = camera->fovy + (targetFOV - camera->fovy) * dt * FOV_SPEED;

  // -----------------------------
  // Movement keys (NOT heat-gated)
  // -----------------------------
  if (!controlsLocked) {
    float totalSpeedMult = isSprinting ? 2.5f : 1.0f;
    float forwardSpeedMult = 5.0f * totalSpeedMult;
    float backwardSpeedMult = 5.0f * totalSpeedMult;

    if (IsKeyDown(KEY_W)) {
      vel[pid].x += forward.x * 100.0f * dt * forwardSpeedMult;
      vel[pid].z += forward.z * 100.0f * dt * forwardSpeedMult;
    }
    if (IsKeyDown(KEY_S)) {
      vel[pid].x -= forward.x * 100.0f * dt * backwardSpeedMult;
      vel[pid].z -= forward.z * 100.0f * dt * backwardSpeedMult;
    }
  }

  // -----------------------------
  // Weapons (heat-gated)
  // -----------------------------
  if (!controlsLocked) {

    // Weapon 0: Left gun (LMB) -> ray 1, cooldown slot 0
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) &&
        eng->actors.cooldowns[pid][0] <= 0.0f &&
        HeatTryAction(gs, HEAT_COST_LMB)) {

      eng->actors.cooldowns[pid][0] = eng->actors.firerate[pid][0];
      QueueSound(soundSys, SOUND_WEAPON_FIRE, pos[pid], 0.4f, 1.0f);

      ApplyTorsoRecoil(&eng->actors.modelCollections[pid], 1, 0.01f,
                       (Vector3){-0.2f, 1.0f, 0});

      Ray *ray = &eng->actors.raycasts[pid][1].ray;
      float muzzleOffset = 15.0f;
      Vector3 fwd = Vector3Normalize(ray->direction);
      Vector3 muzzlePos =
          Vector3Add(ray->position, Vector3Scale(fwd, muzzleOffset));

      FireProjectile(eng, pid, 1, 0, 1);
      SpawnSmoke(eng, muzzlePos);
      spawnParticle(eng, muzzlePos, 0.1, 0);
    }

    // Weapon 1: Right cannon shell (RMB) -> ray 2, cooldown slot 1
    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON) &&
        eng->actors.cooldowns[pid][1] <= 0.0f &&
        HeatTryAction(gs, HEAT_COST_RMB)) {

      eng->actors.cooldowns[pid][1] = eng->actors.firerate[pid][1];
      QueueSound(soundSys, SOUND_WEAPON_FIRE, pos[pid], 0.6f, 0.9f);

      ApplyTorsoRecoil(&eng->actors.modelCollections[pid], 1, 0.18f,
                       (Vector3){-0.15f, 1.0f, 0});

      Ray *ray = &eng->actors.raycasts[pid][2].ray;
      float muzzleOffset = 18.0f;
      Vector3 fwd = Vector3Normalize(ray->direction);
      Vector3 muzzlePos =
          Vector3Add(ray->position, Vector3Scale(fwd, muzzleOffset));

      FireProjectile(eng, pid, 2, 1, 2);
      SpawnSmoke(eng, muzzlePos);
      spawnParticle(eng, muzzlePos, 0.1, 0);
    }

    // Weapon 2: Shoulder rocket (Q) -> ray 3, cooldown slot 2
    if (IsKeyPressed(KEY_Q) && eng->actors.cooldowns[pid][2] <= 0.0f &&
        HeatTryAction(gs, HEAT_COST_ROCKET)) {

      eng->actors.cooldowns[pid][2] = eng->actors.firerate[pid][2];
      QueueSound(soundSys, SOUND_ROCKET_FIRE, pos[pid], 1.0f, 1.1f);

      Ray *ray = &eng->actors.raycasts[pid][3].ray;
      float muzzleOffset = 20.0f;
      Vector3 fwd = Vector3Normalize(ray->direction);
      Vector3 muzzlePos =
          Vector3Add(ray->position, Vector3Scale(fwd, muzzleOffset));

      FireProjectile(eng, pid, 3, 2, 3);
      spawnParticle(eng, muzzlePos, 0.1, 0);
    }

    // Weapon 3: BLUNDERBUSS
    if (IsKeyPressed(KEY_E) && eng->actors.cooldowns[pid][3] <= 0.0f &&
        HeatTryAction(gs, HEAT_COST_LMB)) {

      eng->actors.cooldowns[pid][3] = eng->actors.firerate[pid][2];
      QueueSound(soundSys, SOUND_WEAPON_FIRE, pos[pid], 1.0f, 1.1f);

      Ray *ray = &eng->actors.raycasts[pid][4].ray;

      float muzzleOffset = 20.0f;
      Vector3 baseDir = Vector3Normalize(ray->direction);
      Vector3 muzzlePos =
          Vector3Add(ray->position, Vector3Scale(baseDir, muzzleOffset));

      // --- blunderbuss params ---
      const int pelletCount = 15;   // number of pellets
      const float spreadDeg = 1.4f; // cone half-angle (degrees)
      const float spreadRad = spreadDeg * DEG2RAD;

      // Build an orthonormal basis around baseDir (right/up)
      Vector3 worldUp = (Vector3){0, 1, 0};
      Vector3 right = Vector3CrossProduct(worldUp, baseDir);
      if (Vector3Length(right) < 0.001f) {
        // if aiming almost straight up/down, choose another axis
        worldUp = (Vector3){1, 0, 0};
        right = Vector3CrossProduct(worldUp, baseDir);
      }
      right = Vector3Normalize(right);
      Vector3 up = Vector3Normalize(Vector3CrossProduct(baseDir, right));

      // Save original ray direction so we can restore it
      Vector3 originalDir = ray->direction;

      for (int i = 0; i < pelletCount; i++) {
        // Random offsets in [-spreadRad, +spreadRad]
        float rx = ((float)GetRandomValue(-1000, 1000) / 1000.0f) * spreadRad;
        float ry = ((float)GetRandomValue(-1000, 1000) / 1000.0f) * spreadRad;

        Vector3 pelletDir = Vector3Add(
            baseDir, Vector3Add(Vector3Scale(right, rx), Vector3Scale(up, ry)));
        pelletDir = Vector3Normalize(pelletDir);

        // Temporarily aim the ray this way so FireProjectile uses it
        ray->direction = pelletDir;

        // FireProjectile(eng, owner, weapon?, projectileType?, damage?)
        FireProjectile(eng, pid, 4, 3, 5);
      }

      // Restore aim ray
      ray->direction = originalDir;

      // One muzzle particle (not per pellet)
      spawnParticle(eng, muzzlePos, 0.1, 0);
    }
  }

  // -----------------------------
  // Gun visual recoil offsets
  // NOTE: keep your original behavior, but fix pid indexing
  // -----------------------------
  mc->offsets[2].z = 8.0f - (eng->actors.cooldowns[pid][0]) * 2.0f;
  mc->offsets[3].z = 8.0f - (eng->actors.cooldowns[pid][1]) * 2.0f;
  mc->offsets[5].z = 8.0f - (eng->actors.cooldowns[pid][3]) * 2.0f;

  // -----------------------------
  // Headbob + footsteps disabled during dash
  // -----------------------------
  if (controlsLocked) {
    gs->pHeadbobTimer = 0.0f;
    eng->actors.stepCycle[pid] = 0.0f;
    eng->actors.prevStepCycle[pid] = 0.0f;
    eng->actors.stepRate[pid] = 0.0f;
  } else {
    Vector3 v = vel[pid];
    float speed = sqrtf(v.x * v.x + v.z * v.z);

    if (speed > 1.0f) {
      gs->pHeadbobTimer += dt * 8.0f;
      eng->actors.stepRate[pid] = speed * 0.07f;

      float prev = eng->actors.prevStepCycle[pid];
      float curr = eng->actors.stepCycle[pid] + eng->actors.stepRate[pid] * dt;

      if (curr >= 1.0f)
        curr -= 1.0f;

      if (prev > curr) {
        QueueSound(soundSys, SOUND_FOOTSTEP, pos[pid], 0.1f, 1.0f);
      }

      eng->actors.stepCycle[pid] = curr;
      eng->actors.prevStepCycle[pid] = curr;
    } else {
      gs->pHeadbobTimer = 0.0f;
      eng->actors.stepCycle[pid] = 0.0f;
      eng->actors.prevStepCycle[pid] = 0.0f;
    }
  }
}
