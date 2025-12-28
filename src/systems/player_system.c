
#include "systems.h"
#include <math.h>

#define BOB_AMOUNT 0.5f

extern Vector3 recoilOffset;

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

void PlayerControlSystem(GameState_t *gs, Engine_t *eng,
                         SoundSystem_t *soundSys, float dt, Camera3D *camera) {

  int pid = gs->playerId;

  Vector3 *pos =
      (Vector3 *)GetComponentArray(&eng->actors, gs->compReg.cid_Positions);
  Vector3 *vel =
      (Vector3 *)GetComponentArray(&eng->actors, gs->compReg.cid_velocities);

  // player leg and torso orientations
  Orientation *leg = &eng->actors.modelCollections[pid].orientations[0];
  Orientation *torso = &eng->actors.modelCollections[pid].orientations[1];

  bool isSprinting = IsKeyDown(KEY_LEFT_SHIFT);

  float sensitivity = 0.0007f;

  float baseFOV = eng->config.fov_deg;
  float sprintFOV = baseFOV * 1.1;
  float zoomFOV = 10.0f;
  float fovSpeed = 12.0f; // how fast FOV interpolates

  float targetFOV = isSprinting ? sprintFOV : baseFOV;

  float totalSpeedMult = isSprinting ? 1.5f : 1.0f;
  float forwardSpeedMult = 5.0f * totalSpeedMult;
  float backwardSpeedMult = 5.0f * totalSpeedMult;
  float strafeSpeedMult = 2.0f * totalSpeedMult;

  // TODO fix later ZOOM basic
  if (IsKeyDown(KEY_B)) {
    sensitivity = 0.0002f;
    camera->fovy = 10;
  } else {
    camera->fovy = baseFOV;
    // TODO fix interpolation
    camera->fovy = camera->fovy + (targetFOV - camera->fovy) * dt * fovSpeed;
    sensitivity = 0.0007f;
  }

  float turnRate = isSprinting ? 0.2f : 1.0f;

  // Rotate legs with A/D
  if (IsKeyDown(KEY_A))
    leg[pid].yaw -= 1.5f * dt * turnRate;
  if (IsKeyDown(KEY_D))
    leg[pid].yaw += 1.5f * dt * turnRate;

  // Torso yaw/pitch from mouse
  Vector2 mouse = GetMouseDelta();
  torso[pid].yaw += mouse.x * sensitivity;
  torso[pid].pitch += -mouse.y * sensitivity;
  // gs->entities.collisionCollections[pid].orientations[pid].yaw =
  // torso[pid].yaw;

  UpdateRayDistance(gs, eng, gs->playerId, dt);

  // Clamp torso pitch between -89° and +89°
  if (torso[pid].pitch > 1.2f)
    torso[pid].pitch = 1.2f;
  if (torso[pid].pitch < -1.0f)
    torso[pid].pitch = -1.0f;

  // Movement is based on leg orientation
  float c = cosf(leg[pid].yaw);
  float s = sinf(leg[pid].yaw);
  Vector3 forward = {c, 0, s};
  Vector3 right = {-s, 0, c};

  // Movement keys
  if (IsKeyDown(KEY_SPACE)) {
    vel[pid].y += 100.0f * dt;
  }

  if (IsKeyDown(KEY_W)) {
    vel[pid].x += forward.x * 100.0f * dt * forwardSpeedMult;
    vel[pid].z += forward.z * 100.0f * dt * forwardSpeedMult;
  }
  if (IsKeyDown(KEY_S)) {
    vel[pid].x -= forward.x * 100.0f * dt * backwardSpeedMult;
    vel[pid].z -= forward.z * 100.0f * dt * backwardSpeedMult;
  }
  if (IsKeyDown(KEY_Q)) {
    vel[pid].x += right.x * -100.0f * dt * strafeSpeedMult;
    vel[pid].z += right.z * -100.0f * dt * strafeSpeedMult;
  }
  if (IsKeyDown(KEY_E)) {
    vel[pid].x += right.x * 100.0f * dt * strafeSpeedMult;
    vel[pid].z += right.z * 100.0f * dt * strafeSpeedMult;
  }

  if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) &&
      eng->actors.cooldowns[pid][0] <= 0) {
    printf("firing\n");
    eng->actors.cooldowns[pid][0] = eng->actors.firerate[pid][0];
    QueueSound(soundSys, SOUND_WEAPON_FIRE, pos[pid], 0.4f, 1.0f);

    ApplyTorsoRecoil(&eng->actors.modelCollections[gs->playerId], 1, 0.1f,
                     (Vector3){-0.2f, 1.0f, 0});

    Ray *ray = &eng->actors.raycasts[gs->playerId][1].ray;

    float muzzleOffset = 15.0f; // tune this value depending on your gun model
    Vector3 forward = Vector3Normalize(ray->direction);

    Vector3 muzzlePos =
        Vector3Add(ray->position, Vector3Scale(forward, muzzleOffset));

    FireProjectile(eng, gs->playerId, 1);
    SpawnSmoke(eng, muzzlePos);
  }

  eng->actors.modelCollections[pid].offsets[2].z =
      8.0f - *(eng->actors.cooldowns[pid]);

  // Step cycle update
  Vector3 velocity = vel[pid];
  float speed = sqrtf(velocity.x * velocity.x + velocity.z * velocity.z);

  if (speed > 1.0f) {
    gs->pHeadbobTimer += dt * 8.0f;
    eng->actors.stepRate[pid] = speed * 0.07;

    float prev = eng->actors.prevStepCycle[pid];
    float curr = eng->actors.stepCycle[pid] + eng->actors.stepRate[pid] * dt;

    if (curr >= 1.0f)
      curr -= 1.0f; // wrap cycle

    if (prev > curr) { // wrapped around -> stomp
      QueueSound(soundSys, SOUND_FOOTSTEP, pos[pid], 0.2f, 1.0f);
    }

    eng->actors.stepCycle[pid] = curr;
    eng->actors.prevStepCycle[pid] = curr;
  } else {
    gs->pHeadbobTimer = 0;
    eng->actors.stepCycle[pid] = 0.0f;
    eng->actors.prevStepCycle[pid] = 0.0f;
  }
}
