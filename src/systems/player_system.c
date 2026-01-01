
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
static float HEAT_COST_LMB = 3.0f;     // per shot
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
  printf("cooled mech, heat at %f\n", HeatGet(gs));
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

  Vector3 *pos =
      (Vector3 *)GetComponentArray(&eng->actors, gs->compReg.cid_Positions);
  Vector3 *vel =
      (Vector3 *)GetComponentArray(&eng->actors, gs->compReg.cid_velocities);

  // Reuse moveBehaviour/moveTimer for dash state
  int *pState =
      (int *)getComponent(&eng->actors, pid, gs->compReg.cid_moveBehaviour);
  float *pTimer =
      (float *)getComponent(&eng->actors, pid, gs->compReg.cid_moveTimer);

  if (!pos || !vel || !pState || !pTimer)
    return;

  // player leg and torso orientations
  ModelCollection_t *mc = &eng->actors.modelCollections[pid];
  if (!mc->orientations || mc->countModels < 2)
    return;

  Orientation *leg = &mc->orientations[0];
  Orientation *torso = &mc->orientations[1];

  bool isSprinting = IsKeyDown(KEY_LEFT_SHIFT);

  // -----------------------------
  // Dash trigger
  // -----------------------------
  if (IsKeyPressed(KEY_SPACE) && *pState == PSTATE_NORMAL) {
    // if (HeatTryAction(gs, HEAT_COST_DASH)) {
    // *pState = PSTATE_DASH_CHARGE;
    // *pTimer = DASH_CHARGE_TIME;
    // }

    *pState = PSTATE_DASH_CHARGE;
    *pTimer = DASH_CHARGE_TIME;
  }

  bool controlsLocked = (*pState != PSTATE_NORMAL);

  // -----------------------------
  // DASH STATE MACHINE (movement locked)
  // -----------------------------
  switch ((PlayerMoveState)(*pState)) {
  case PSTATE_NORMAL:
    break;

  case PSTATE_DASH_CHARGE: {
    *pTimer -= dt;

    // optional: soften existing motion while charging
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

  // -----------------------------
  // Rotation + mouse look (locked during dash)
  // -----------------------------
  float turnRate = isSprinting ? 0.2f : 1.0f;
  float sensitivity = 0.0007f;

  // zoom basic
  if (IsKeyDown(KEY_B)) {
    sensitivity = 0.0002f;
  }

  if (!controlsLocked) {
    HeatCool(gs, dt, HEAT_COOL_PER_SEC);

    if (IsKeyDown(KEY_A))
      leg->yaw -= 1.5f * dt * turnRate;
    if (IsKeyDown(KEY_D))
      leg->yaw += 1.5f * dt * turnRate;

    Vector2 mouse = GetMouseDelta();
    torso->yaw += mouse.x * sensitivity;
    torso->pitch += -mouse.y * sensitivity;
  }

  UpdateRayDistance(gs, eng, gs->playerId, dt);

  // -----------------------------
  // Torso pitch kick (quick up then back down)
  // -----------------------------
  // Smooth blend 0..1 based on dash state. This avoids permanent drift.
  static float kickBlend = 0.0f;
  float targetBlend = 0.0f;

  if (*pState == PSTATE_DASH_CHARGE)
    targetBlend = 1.0f; // snap up quickly
  else if (*pState == PSTATE_DASH_GO)
    targetBlend = 0.6f; // hold some kick during dash
  else if (*pState == PSTATE_DASH_SLOW)
    targetBlend = 0.25f; // start returning
  else
    targetBlend = 0.0f; // return to normal

  float k = (targetBlend > kickBlend) ? KICK_EASE_IN : KICK_EASE_OUT;
  kickBlend += (targetBlend - kickBlend) * (1.0f - expf(-k * dt));

  // Apply as additive "pose", not accumulating forever.
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
  Vector3 right = (Vector3){-s, 0, c};

  // -----------------------------
  // FOV (slight increase during dash)
  // -----------------------------
  float baseFOV = eng->config.fov_deg;
  float sprintFOV = baseFOV * 1.1f;
  float targetFOV = isSprinting ? sprintFOV : baseFOV;

  if (controlsLocked) {
    targetFOV *= DASH_FOV_MULT;
  }

  if (IsKeyDown(KEY_B)) {
    // keep your zoom behavior
    targetFOV = 10.0f;
  }

  camera->fovy = camera->fovy + (targetFOV - camera->fovy) * dt * FOV_SPEED;

  // -----------------------------
  // Movement keys (locked during dash)
  // -----------------------------
  if (!controlsLocked) {
    float totalSpeedMult = isSprinting ? 1.5f : 1.0f;
    float forwardSpeedMult = 5.0f * totalSpeedMult;
    float backwardSpeedMult = 5.0f * totalSpeedMult;
    float strafeSpeedMult = 2.0f * totalSpeedMult;

    bool wantsMove = IsKeyDown(KEY_W) || IsKeyDown(KEY_S);

    if (IsKeyDown(KEY_W)) {
      vel[pid].x += forward.x * 100.0f * dt * forwardSpeedMult;
      vel[pid].z += forward.z * 100.0f * dt * forwardSpeedMult;
    }
    if (IsKeyDown(KEY_S)) {
      vel[pid].x -= forward.x * 100.0f * dt * backwardSpeedMult;
      vel[pid].z -= forward.z * 100.0f * dt * backwardSpeedMult;
    }

    // -----------------------------
    // WEAPON 0: Left gun (LMB)  -> ray 1, cooldown slot 0
    // -----------------------------
    if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) &&
        eng->actors.cooldowns[pid][0] <= 0.0f) {

      eng->actors.cooldowns[pid][0] = eng->actors.firerate[pid][0];
      QueueSound(soundSys, SOUND_WEAPON_FIRE, pos[pid], 0.4f, 1.0f);

      ApplyTorsoRecoil(&eng->actors.modelCollections[pid], 1, 0.01f,
                       (Vector3){-0.2f, 1.0f, 0});

      Ray *ray = &eng->actors.raycasts[pid][1].ray; // left muzzle
      float muzzleOffset = 15.0f;
      Vector3 fwd = Vector3Normalize(ray->direction);
      Vector3 muzzlePos =
          Vector3Add(ray->position, Vector3Scale(fwd, muzzleOffset));

      FireProjectile(eng, pid, 1, 0, 1); // ray index 1
      SpawnSmoke(eng, muzzlePos);
    }

    // -----------------------------
    // WEAPON 1: Right cannon shell (RMB) -> ray 2, cooldown slot 1
    // -----------------------------
    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON) &&
        eng->actors.cooldowns[pid][1] <= 0.0f) {

      eng->actors.cooldowns[pid][1] = eng->actors.firerate[pid][1];
      QueueSound(soundSys, SOUND_WEAPON_FIRE, pos[pid], 0.6f,
                 0.9f); // tweak sound

      ApplyTorsoRecoil(&eng->actors.modelCollections[pid], 1, 0.18f,
                       (Vector3){-0.15f, 1.0f, 0});

      Ray *ray = &eng->actors.raycasts[pid][2].ray; // right muzzle
      float muzzleOffset = 18.0f;                   // slightly longer
      Vector3 fwd = Vector3Normalize(ray->direction);
      Vector3 muzzlePos =
          Vector3Add(ray->position, Vector3Scale(fwd, muzzleOffset));

      FireProjectile(eng, pid, 2, 1, 2); // ray index 2 (big shell)
      SpawnSmoke(eng, muzzlePos);
    }

    // -----------------------------
    // WEAPON 2: Shoulder rocket (Q press) -> ray 3, cooldown slot 2
    // -----------------------------
    if (IsKeyPressed(KEY_Q) && eng->actors.cooldowns[pid][2] <= 0.0f) {

      eng->actors.cooldowns[pid][2] = eng->actors.firerate[pid][2];
      QueueSound(soundSys, SOUND_ROCKET_FIRE, pos[pid], 1.0f,
                 1.1f); // tweak sound

      Ray *ray = &eng->actors.raycasts[pid][3].ray; // shoulder muzzle
      float muzzleOffset = 20.0f;
      Vector3 fwd = Vector3Normalize(ray->direction);
      Vector3 muzzlePos =
          Vector3Add(ray->position, Vector3Scale(fwd, muzzleOffset));

      FireProjectile(eng, pid, 3, 2, 3); // ray index 3 (rocket)
    }
  }

  eng->actors.modelCollections[pid].offsets[2].z =
      8.0f - (eng->actors.cooldowns[0][0]) * 2;
  eng->actors.modelCollections[pid].offsets[3].z =
      8.0f - (eng->actors.cooldowns[0][1]) * 2;

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

      if (prev > curr) { // wrapped around -> stomp
        QueueSound(soundSys, SOUND_FOOTSTEP, pos[pid], 0.2f, 1.0f);
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
