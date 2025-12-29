#include "raylib.h"
#include "raymath.h"
#include "systems.h"
#include <math.h>
#include <stdlib.h>

// Returns true if the ray intersects a sphere around the player within maxDist.
static bool RayAimsAtPlayer(Ray ray, Vector3 playerPos, float playerRadius,
                            float maxDist) {
  RayCollision hit = GetRayCollisionSphere(ray, playerPos, playerRadius);
  return hit.hit && (hit.distance <= maxDist);
}

static bool BarrelAimingAtPlayer(Vector3 barrelPos, Vector3 barrelDir,
                                 Vector3 playerPos, float maxAngleDeg) {
  Vector3 toPlayer = Vector3Normalize(Vector3Subtract(playerPos, barrelPos));
  float d = Vector3DotProduct(barrelDir, toPlayer);
  d = fminf(1.0f, fmaxf(-1.0f, d));
  float angle = acosf(d) * RAD2DEG;
  return angle <= maxAngleDeg;
}

// Helper function to get a random point on a circle around a center position
// The point will be within 15 degrees of the direction from entity to center
Vector3 GetRandomPointAroundPosition(Vector3 entityPos, Vector3 center,
                                     float maxRadius) {
  Vector3 result;

  Vector3 toCenter;
  toCenter.x = center.x - entityPos.x;
  toCenter.z = center.z - entityPos.z;
  toCenter.y = 0.0f;

  float baseAngle = atan2f(toCenter.z, toCenter.x);

  float angleOffset = ((float)rand() / RAND_MAX) * 0.523598f - 0.261799f +
                      PI / 2; // +-15 degrees
  float angle = baseAngle + angleOffset;

  float radius =
      ((float)rand() / RAND_MAX) * maxRadius + 500; // Random radius 0-maxRadius

  float xOffset = cosf(angle) * radius;
  float zOffset = sinf(angle) * radius;

  result.x = center.x + xOffset;
  result.y = 0.0f; // Keep y = 0
  result.z = center.z + zOffset;

  return result;
}

// Helper function to get a random point straight towards the player
Vector3 GetPointTowardsPlayer(Vector3 entityPos, Vector3 playerPos,
                              float distance) {
  Vector3 result;

  Vector3 direction;
  direction.x = playerPos.x - entityPos.x;
  direction.z = playerPos.z - entityPos.z;
  direction.y = 0.0f;

  float length = sqrtf(direction.x * direction.x + direction.z * direction.z);
  if (length > 0) {
    direction.x /= length;
    direction.z /= length;
  }

  result.x = entityPos.x + direction.x * distance;
  result.z = entityPos.z + direction.z * distance;
  result.y = 0.0f;

  return result;
}

// Helper function to get a random point away from the player
Vector3 GetPointAwayFromPlayer(Vector3 entityPos, Vector3 playerPos,
                               float distance) {
  Vector3 result;

  Vector3 direction;
  direction.x = entityPos.x - playerPos.x;
  direction.z = entityPos.z - playerPos.z;
  direction.y = 0.0f;

  float length = sqrtf(direction.x * direction.x + direction.z * direction.z);
  if (length > 0) {
    direction.x /= length;
    direction.z /= length;
  }

  result.x = entityPos.x + direction.x * distance;
  result.z = entityPos.z + direction.z * distance;
  result.y = 0.0f;

  return result;
}

static float DistXZ(Vector3 a, Vector3 b) {
  float dx = a.x - b.x;
  float dz = a.z - b.z;
  return sqrtf(dx * dx + dz * dz);
}

static bool PlayerInDetectionZone(Vector3 playerPos) {
  return DistXZ(playerPos, DETECTION_CENTER) <= DETECTION_RADIUS;
}

// Pick a point on a circle around the player, with a tangential offset
static Vector3 GetCirclePointAroundPlayer(Vector3 tankPos, Vector3 playerPos,
                                          float radius) {
  Vector3 toTank = Vector3Subtract(tankPos, playerPos);
  toTank.y = 0.0f;

  float len = sqrtf(toTank.x * toTank.x + toTank.z * toTank.z);
  if (len < 0.001f) {
    toTank = (Vector3){1, 0, 0};
    len = 1.0f;
  }
  toTank.x /= len;
  toTank.z /= len;

  // Tangent direction for circling
  Vector3 tangent = (Vector3){-toTank.z, 0.0f, toTank.x};

  // Bias forward along tangent so it actually circles instead of orbit-stalling
  float forward = 250.0f; // tweak "orbit speed"
  Vector3 target = Vector3Add(playerPos, Vector3Scale(toTank, radius));
  target = Vector3Add(target, Vector3Scale(tangent, forward));
  target.y = 0.0f;
  return target;
}

void UpdateEnemyTargets(GameState_t *gs, Engine_t *eng, SoundSystem_t *soundSys,
                        float dt) {
  int emCount = eng->em.count;
  Vector3 *playerPos =
      getComponent(&eng->actors, gs->playerId, gs->compReg.cid_Positions);
  if (!playerPos)
    return;

  for (int i = 0; i < emCount; i++) {
    if (!eng->em.alive[i])
      continue;
    if (eng->actors.types[i] != ENTITY_TANK)
      continue;

    uint32_t mask = eng->em.masks[i];
    if (!(mask & C_TANK_MOVEMENT))
      continue;

    Vector3 *pos = getComponent(&eng->actors, i, gs->compReg.cid_Positions);
    Vector3 *moveTarget =
        getComponent(&eng->actors, i, gs->compReg.cid_moveTarget);
    int *state =
        (int *)getComponent(&eng->actors, i, gs->compReg.cid_moveBehaviour);

    float *stateTimer =
        (float *)getComponent(&eng->actors, i, gs->compReg.cid_moveTimer);
    // If you add a dedicated ai timer component, use that instead.

    if (!pos || !moveTarget || !state || !stateTimer)
      continue;

    bool detected = PlayerInDetectionZone(*playerPos);

    // --- state transitions based on detection ---
    if (!detected) {
      // go idle
      // printf("not detected: IDLING\n");
      *state = TANK_IDLE;
    } else {
      if (*state == TANK_IDLE) {
        // printf("detected: circling\n");
        *state = TANK_ALERT_CIRCLE;
        *stateTimer = CHARGE_COOLDOWN; // time until first charge
      }
    }

    // printf("state = %d\n", (int)(*state));
    // --- state behavior ---
    switch ((TankAIState)(*state)) {
    case TANK_IDLE: {
      // move to fixed idle point
      Vector3 idle = IDLE_POINT;
      idle.y = 0.0f;
      *moveTarget = idle;

      // keep timer reset-ish
      *stateTimer = 0.0f;
    } break;

    case TANK_ALERT_CIRCLE: {

      *moveTarget = GetCirclePointAroundPlayer(*pos, *playerPos, CIRCLE_RADIUS);

      // countdown to next charge
      *stateTimer -= dt;
      if (*stateTimer <= 0.0f) {
        *state = TANK_ALERT_CHARGE;
        *stateTimer = CHARGE_DURATION;
      }
    } break;

    case TANK_ALERT_CHARGE: {
      // run straight at player
      *moveTarget = GetPointTowardsPlayer(*pos, *playerPos, 1000.0f);

      *stateTimer -= dt;
      if (*stateTimer <= 0.0f) {
        *state = TANK_ALERT_CIRCLE;
        *stateTimer = CHARGE_COOLDOWN;
      }
    } break;
    }
  }
}

void UpdateEnemyVelocities(GameState_t *gs, Engine_t *eng,
                           SoundSystem_t *soundSys, float dt) {
  int emCount = eng->em.count;

  for (int i = 0; i < emCount; i++) {
    if (!eng->em.alive[i])
      continue;

    EntityType_t type = eng->actors.types[i];
    if (!(type == ENTITY_MECH || type == ENTITY_TURRET || type == ENTITY_TANK))
      continue;

    uint32_t mask = eng->em.masks[i];

    // Only update velocity for entities that can move
    if (mask & C_TANK_MOVEMENT) {
      // Get required components
      Vector3 *position =
          getComponent(&eng->actors, i, gs->compReg.cid_Positions);
      Vector3 *velocity =
          getComponent(&eng->actors, i, gs->compReg.cid_velocities);
      Vector3 *moveTarget =
          getComponent(&eng->actors, i, gs->compReg.cid_moveTarget);
      int *moveBehaviour =
          (int *)getComponent(&eng->actors, i, gs->compReg.cid_moveBehaviour);
      float moveSpeed = 50;

      position->y =
          GetTerrainHeightAtXZ(&gs->terrain, position->x, position->z);
      if (!position || !velocity || !moveTarget || !moveBehaviour || !moveSpeed)
        continue;

      Vector3 direction;
      direction.x = moveTarget->x - position->x;
      direction.y = 0.0f; // only moving in horizontal plane
      direction.z = moveTarget->z - position->z;

      ModelCollection_t *mc = &eng->actors.modelCollections[i];

      float moveYaw = atan2f(direction.x, direction.z);

      mc->localRotationOffset[0].yaw = moveYaw;

      // Normalize the direction (if not zero)
      float distanceSquared =
          direction.x * direction.x + direction.z * direction.z;

      float adjustedMoveSpeed = moveSpeed;
      if (*moveBehaviour == TANK_ALERT_CHARGE) {
        adjustedMoveSpeed = moveSpeed * 1.5f; // 50% faster for charge/retreat
      }

      // Only move if we're not already at the target
      if (distanceSquared > 1.0f) { // Small threshold
        float distance = sqrtf(distanceSquared);
        direction.x /= distance;
        direction.z /= distance;

        velocity->x = direction.x * adjustedMoveSpeed;
        velocity->y = 0.0f;
        velocity->z = direction.z * adjustedMoveSpeed;
      } else {
        velocity->x = 0.0f;
        velocity->z = 0.0f;
      }
      velocity->y = 0.0f;
    }
  }
}

// Helper function to get direction from tank to player as a normalized vector
Vector3 GetDirectionToPlayer(Vector3 tankPos, Vector3 playerPos) {
  Vector3 direction = Vector3Subtract(playerPos, tankPos);
  return Vector3Normalize(direction);
}

// Calculate the actual aiming point (player position + optional offset for
// leading shots)
Vector3 CalculateAimTarget(Vector3 tankPos, Vector3 playerPos,
                           Vector3 playerVel, float projectileSpeed) {
  // For now, just return player position directly
  // Later you can add leading/trajectory prediction here
  return playerPos;
}

static Vector3 AimStraightAheadTarget(int tankId, Engine_t *eng,
                                      Vector3 tankPos) {
  Vector3 fwd = GetTankForwardDirection(tankId, eng);
  Vector3 t = Vector3Add(tankPos, Vector3Scale(fwd, 1000.0f)); // far point
  return t;
}

void UpdateTankAimingAndShooting(GameState_t *gs, Engine_t *eng,
                                 SoundSystem_t *soundSys, float dt) {
  int emCount = eng->em.count;

  // Get player position
  Vector3 *playerPos =
      getComponent(&eng->actors, gs->playerId, gs->compReg.cid_Positions);
  if (!playerPos)
    return;

  // y offset
  (*playerPos).y -= 3.0;

  Vector3 *playerVel =
      getComponent(&eng->actors, gs->playerId, gs->compReg.cid_velocities);
  Vector3 playerVelocity = {0, 0, 0};
  if (playerVel) {
    playerVelocity = *playerVel;
  }

  for (int i = 0; i < emCount; i++) {
    if (!eng->em.alive[i])
      continue;

    EntityType_t type = eng->actors.types[i];
    if (type != ENTITY_TANK)
      continue;

    uint32_t mask = eng->em.masks[i];
    if (!(mask & C_TURRET_BEHAVIOUR_1))
      continue;

    // Get tank position
    Vector3 *tankPos = getComponent(&eng->actors, i, gs->compReg.cid_Positions);
    if (!tankPos)
      continue;

    // Get aim target component
    Vector3 *aimTarget =
        getComponent(&eng->actors, i, gs->compReg.cid_aimTarget);
    if (!aimTarget)
      continue;

    // Get other components we might need
    float *aimError =
        (float *)getComponent(&eng->actors, i, gs->compReg.cid_aimError);
    float *cooldown = eng->actors.cooldowns[i];
    float *firerate = eng->actors.firerate[i];
    float *muzzleVelocity = eng->actors.muzzleVelocities[i];

    // Calculate simple aim target (player position)
    float projectileSpeed = muzzleVelocity ? muzzleVelocity[0] : 50.0f;
    Vector3 calculatedTarget = CalculateAimTarget(
        *tankPos, *playerPos, playerVelocity, projectileSpeed);

    // Add some randomness/error to aiming
    if (aimError && *aimError > 0) {
      float errorAmount = *aimError * 5.0f; // Scale the error
      calculatedTarget.x +=
          ((float)rand() / RAND_MAX) * 2.0f * errorAmount - errorAmount;
      calculatedTarget.y +=
          ((float)rand() / RAND_MAX) * 2.0f * errorAmount - errorAmount;
      calculatedTarget.z +=
          ((float)rand() / RAND_MAX) * 2.0f * errorAmount - errorAmount;
    }

    int *state =
        (int *)getComponent(&eng->actors, i, gs->compReg.cid_moveBehaviour);

    // if (*state == TANK_IDLE) {
    //   calculatedTarget = AimStraightAheadTarget(i, eng, *tankPos);
    //   *aimTarget = calculatedTarget;
    //   continue;
    // }
    *aimTarget = calculatedTarget;

    // DEBUG: Print the aim target
    // printf("Tank %d aiming at: (%.2f, %.2f, %.2f)\n", i,
    //        aimTarget->x, aimTarget->y, aimTarget->z);
  }
}

static Vector3 ForwardFromYawPitch(float yaw, float pitch) {
  // yaw around Y, pitch around X (raylib convention)
  Vector3 f;
  f.x = sinf(yaw) * cosf(pitch);
  f.y = sinf(pitch);
  f.z = cosf(yaw) * cosf(pitch);
  return Vector3Normalize(f);
}

// Helper function to get tank's forward direction from its base orientation
Vector3 GetTankForwardDirection(int tankId, Engine_t *eng) {
  Vector3 forward = {0, 0, 1}; // Default forward (Z axis)

  if (tankId < 0 || tankId >= MAX_ENTITIES)
    return forward;

  ModelCollection_t *mc = &eng->actors.modelCollections[tankId];
  if (mc->countModels < 1 || !mc->orientations)
    return forward;

  // Get the base model (index 0) orientation
  Orientation *baseOri = &mc->orientations[0];

  // Calculate forward vector from yaw
  forward.x = sinf(baseOri->yaw);
  forward.z = cosf(baseOri->yaw);
  forward.y = 0;

  return Vector3Normalize(forward);
}

void UpdateTankTurretAiming(GameState_t *gs, Engine_t *eng,
                            SoundSystem_t *soundSys, float dt) {
  int emCount = eng->em.count;

  Vector3 *playerPos =
      getComponent(&eng->actors, gs->playerId, gs->compReg.cid_Positions);
  if (!playerPos)
    return;

  for (int i = 0; i < emCount; i++) {
    if (!eng->em.alive[i])
      continue;
    if (eng->actors.types[i] != ENTITY_TANK)
      continue;

    // Get tank position and aim target
    Vector3 *tankPos = getComponent(&eng->actors, i, gs->compReg.cid_Positions);
    Vector3 *aimTarget =
        getComponent(&eng->actors, i, gs->compReg.cid_aimTarget);

    if (!tankPos || !aimTarget)
      continue;

    // Get the model collection
    ModelCollection_t *mc = &eng->actors.modelCollections[i];
    if (mc->countModels < 3) // Need base(0), turret(1), barrel(2)
      continue;

    // Ensure localRotationOffset array exists
    if (!mc->localRotationOffset)
      continue;

    // Calculate direction from tank to aim target
    Vector3 direction = Vector3Subtract(*aimTarget, *tankPos);

    // Calculate yaw (horizontal rotation) - around Y axis
    float targetYaw = atan2f(direction.x, direction.z);

    // Calculate pitch (vertical rotation) - around X axis
    float horizontalDist =
        sqrtf(direction.x * direction.x + direction.z * direction.z);
    float targetPitch = -atan2f(direction.y, horizontalDist);

    // Smooth interpolation for nicer aiming
    float rotationSpeed = 2.5f; // radians per second
    float maxRotation = rotationSpeed * dt;

    // --- TURRET (model 1): Yaw rotation only ---
    // For turret, we want independent yaw rotation (not locked to parent)
    // Based on your transformation code, we should set localRotationOffset.yaw

    float baseYaw = mc->localRotationOffset[0].yaw; // hull yaw (local)
    float targetYawWorld = atan2f(direction.x, direction.z);
    float targetYawLocal = targetYawWorld - baseYaw;

    // wrap targetYawLocal into [-PI, PI] if you want
    while (targetYawLocal > PI)
      targetYawLocal -= 2 * PI;
    while (targetYawLocal < -PI)
      targetYawLocal += 2 * PI;

    // smooth toward targetYawLocal
    float currentTurretYaw = mc->localRotationOffset[1].yaw;
    float yawDiff = targetYawLocal - currentTurretYaw;

    while (yawDiff > PI)
      yawDiff -= 2 * PI;
    while (yawDiff < -PI)
      yawDiff += 2 * PI;

    yawDiff = fminf(fmaxf(yawDiff, -maxRotation), maxRotation);
    mc->localRotationOffset[1].yaw = currentTurretYaw + yawDiff;

    // Turret should have yaw rotation only (no pitch/roll)
    mc->localRotationOffset[1].pitch = 0;
    mc->localRotationOffset[1].roll = 0;

    // --- BARREL (model 2): Pitch rotation only ---
    // Barrel should inherit yaw from parent turret, so we only control pitch
    // Barrel initial state: pointing up (PI/2) in model space
    // We want to rotate from up to aim at target

    float currentBarrelPitch = mc->localRotationOffset[2].pitch;

    float targetBarrelPitch = -targetPitch;

    // Clamp barrel pitch to prevent going too far

    // Smooth rotation
    float pitchDiff = targetBarrelPitch - currentBarrelPitch;
    pitchDiff = fminf(fmaxf(pitchDiff, -maxRotation), maxRotation);
    mc->localRotationOffset[2].pitch = currentBarrelPitch + pitchDiff;

    mc->localRotationOffset[2].yaw = 0;
    mc->localRotationOffset[2].roll = 0;

    // Also update orientations array to keep them in sync
    if (mc->orientations) {
      mc->orientations[1].yaw = mc->localRotationOffset[1].yaw;
      mc->orientations[1].pitch = 0;
      mc->orientations[1].roll = 0;

      mc->orientations[2].yaw = 0;
      mc->orientations[2].pitch = mc->localRotationOffset[2].pitch;
      mc->orientations[2].roll = 0;
    }

    // Update the ray direction for shooting
    if (eng->actors.rayCounts[i] > 0) {
      // Find ray attached to barrel (model 2)
      for (int rayIdx = 0; rayIdx < eng->actors.rayCounts[i]; rayIdx++) {
        Raycast_t *raycast = &eng->actors.raycasts[i][rayIdx];
        if (raycast->parentModelIndex == 2) { // Barrel model

          // We need an aim point. Use your turret aim target if you have it.
          // (Replace aimTargetPos with your actual variable.)
          Vector3 aimPos = *aimTarget;

          // Barrel origin in world (where projectile spawns)
          Vector3 muzzlePos = mc->globalPositions[2];

          // Weapon params
          int gunId = 0;
          float muzzleVel = eng->actors.muzzleVelocities[i][gunId]
                                ? eng->actors.muzzleVelocities[i][gunId]
                                : 10.0f;
          float dropRate = eng->actors.dropRates[i][gunId]
                               ? eng->actors.dropRates[i][gunId]
                               : 1.0f;

          // Horizontal distance to aim point (ignore vertical)
          Vector3 toTarget = Vector3Subtract(aimPos, muzzlePos);
          float dxz = sqrtf(toTarget.x * toTarget.x + toTarget.z * toTarget.z);

          // Predict drop and aim higher.
          // (Clamp dxz to avoid division issues when very close)
          if (dxz < 0.001f)
            dxz = 0.001f;

          // Approx time of flight using horizontal distance
          float t = dxz / muzzleVel;

          // Vertical drop over that time (units must match your projectile sim)
          float drop = 1.2f * dropRate * t * t;

          // Aim ABOVE the target by the expected drop
          aimPos.y += drop;

          // Now compute yaw/pitch from muzzle to this compensated aim point
          Vector3 dirToAim =
              Vector3Normalize(Vector3Subtract(aimPos, muzzlePos));

          float turretYawWorld = atan2f(dirToAim.x, dirToAim.z);

          // If your ForwardFromYawPitch expects pitch-from-horizontal
          float pitchFromHorizontal =
              asinf(dirToAim.y); // because dir is normalized

          Vector3 barrelDirWorld =
              ForwardFromYawPitch(turretYawWorld, pitchFromHorizontal);

          // printf("barrelPitchLocal=%.3f (%.1f deg), pitchFromHorizontal=%.3f
          // "
          //        "(%.1f deg)\n",
          //        barrelPitchLocal, barrelPitchLocal * RAD2DEG,
          //        pitchFromHorizontal, pitchFromHorizontal * RAD2DEG);

          // printf("barrelDirWorld: (%.3f, %.3f, %.3f)\n", barrelDirWorld.x,
          //        barrelDirWorld.y, barrelDirWorld.z);

          raycast->ray.position = mc->globalPositions[2];
          raycast->ray.direction = barrelDirWorld;
          raycast->active = true;
          break;
        }
      }
    }

    // // Debug output - less frequent
    // static int debugCounter = 0;
    // debugCounter++;
    // if (debugCounter % 30 == 0) { // Print every ~3 seconds at 60fps
    //   printf("\n=== Tank %d Aiming ===\n", i);
    //   printf("Position: (%.1f, %.1f, %.1f)\n", tankPos->x, tankPos->y,
    //          tankPos->z);
    //   printf("Target: (%.1f, %.1f, %.1f)\n", aimTarget->x, aimTarget->y,
    //          aimTarget->z);
    //   printf("Distance: %.1f\n", Vector3Distance(*tankPos, *aimTarget));

    //   printf("Turret localRotationOffset.yaw: %.1f°\n",
    //          mc->localRotationOffset[1].yaw * RAD2DEG);
    //   printf("Barrel localRotationOffset.pitch: %.1f° (from vertical)\n",
    //          mc->localRotationOffset[2].pitch * RAD2DEG);
    //   printf("Target Pitch (from horizontal): %.1f°\n", targetPitch *
    //   RAD2DEG); printf("Calculated: %.1f° - %.1f° = %.1f°\n", (PI / 2) *
    //   RAD2DEG,
    //          targetPitch * RAD2DEG, ((PI / 2) - targetPitch) * RAD2DEG);
    // }
  }

  for (int i = 0; i < emCount; i++) {
    if (!eng->em.alive[i])
      continue;
    if (eng->actors.types[i] != ENTITY_TANK)
      continue;

    uint32_t mask = eng->em.masks[i];
    if (!(mask & C_TURRET_BEHAVIOUR_1))
      continue;

    float *cooldown = eng->actors.cooldowns[i];
    float *firerate = eng->actors.firerate[i];

    // If you store cooldown as a single float, make sure it's valid
    if (!cooldown)
      continue;

    // Tick cooldown down
    if (*cooldown > 0.0f) {
      *cooldown -= dt;
      continue;
    }

    int *state =
        (int *)getComponent(&eng->actors, i, gs->compReg.cid_moveBehaviour);
    if (!state || *state == TANK_IDLE)
      continue; // never shoot in idle

    // Find the barrel ray index (parentModelIndex == 2), same as your aiming
    // code :contentReference[oaicite:4]{index=4}
    int barrelRayIdx = -1;
    for (int rayIdx = 0; rayIdx < eng->actors.rayCounts[i]; rayIdx++) {
      Raycast_t *rc = &eng->actors.raycasts[i][rayIdx];
      if (rc->active && rc->parentModelIndex == 2) {
        barrelRayIdx = rayIdx;
        break;
      }
    }

    if (barrelRayIdx < 0)
      continue;

    Ray ray = eng->actors.raycasts[i][barrelRayIdx].ray;

    // Decide when “aiming at player” counts
    const float playerRadius = 20.0f;  // tweak
    const float maxShootDist = 500.0f; // tweak
    if (!BarrelAimingAtPlayer(ray.position, ray.direction, *playerPos, 10))
      continue;

    // FIRE
    Vector3 shooterPos =
        *(Vector3 *)getComponent(&eng->actors, i, gs->compReg.cid_Positions);
    QueueSound(soundSys, SOUND_WEAPON_FIRE, shooterPos, 0.4f, 1.0f);

    FireProjectile(eng, (entity_t)i, barrelRayIdx);

    // Smoke at muzzle: start at barrel ray origin, move a bit forward along
    // barrel dir
    const float muzzleOffset = 2.0f; // tweak to match your barrel length
    Vector3 muzzlePos =
        Vector3Add(ray.position, Vector3Scale(ray.direction, muzzleOffset));
    SpawnSmoke(eng, muzzlePos);

    // Reset cooldown from firerate (shots per second). If missing, default 1
    // shot/sec.
    float shotsPerSec = (firerate) ? firerate[0] : 1.0f;
    if (shotsPerSec <= 0.0f)
      shotsPerSec = 1.0f;
    *cooldown = 1.0f / shotsPerSec;
  }
}
