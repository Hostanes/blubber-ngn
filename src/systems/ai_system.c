#include "systems.h"
#include <math.h>
#include <stdlib.h>

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

void UpdateEnemyTargets(GameState_t *gs, Engine_t *eng, SoundSystem_t *soundSys,
                        float dt) {
  int emCount = eng->em.count;
  Vector3 *playerPos =
      getComponent(&eng->actors, gs->playerId, gs->compReg.cid_Positions);

  for (int i = 0; i < emCount; i++) {
    if (!eng->em.alive[i])
      continue;

    EntityType_t type = eng->actors.types[i];
    if (!(type == ENTITY_MECH || type == ENTITY_TURRET || type == ENTITY_TANK))
      continue;

    Vector3 *pos = getComponent(&eng->actors, i, gs->compReg.cid_Positions);

    uint32_t mask = eng->em.masks[i];
    if (mask & C_TANK_MOVEMENT) {
      float *timer =
          (float *)getComponent(&eng->actors, i, gs->compReg.cid_moveTimer);
      Vector3 *moveTarget =
          getComponent(&eng->actors, i, gs->compReg.cid_moveTarget);
      int *moveBehaviour =
          (int *)getComponent(&eng->actors, i, gs->compReg.cid_moveBehaviour);

      if (*timer <= 0) {
        *timer = 10.0f; // Reset timer

        // Randomly switch behavior with 20% chance each time timer resets
        if ((rand() % 100) < 50) {
          *moveBehaviour = (rand() % 3) + 1; // 1, 2, or 3
          printf("Switched behaviour to %d\n", *moveBehaviour);
          if (*moveBehaviour > 1) {
            *timer = 5.0f;
          }
        }

        // Get new target position based on current behavior
        switch (*moveBehaviour) {
        case 1: // Circle around player
          *moveTarget = GetRandomPointAroundPosition(*pos, *playerPos, 500.0f);
          break;

        case 2: // Run straight at player
          *moveTarget = GetPointTowardsPlayer(*pos, *playerPos, 1000.0f);
          break;

        case 3: // Run away from player
          *moveTarget = GetPointAwayFromPlayer(*pos, *playerPos, 1000.0f);
          break;

        default:
          *moveTarget = GetRandomPointAroundPosition(*pos, *playerPos, 500.0f);
          break;
        }
      } else {
        *timer -= dt; // Decrement timer
      }
    }

    if (mask & C_TURRET_BEHAVIOUR_1) {
      // Turret behavior logic here
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

      // Normalize the direction (if not zero)
      float distanceSquared =
          direction.x * direction.x + direction.z * direction.z;

      float adjustedMoveSpeed = moveSpeed;
      if (*moveBehaviour == 2 || *moveBehaviour == 3) {
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

    if (mask & C_TURRET_BEHAVIOUR_1) {
      // Turret velocity logic if needed
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

void UpdateTankAimingAndShooting(GameState_t *gs, Engine_t *eng,
                                 SoundSystem_t *soundSys, float dt) {
  int emCount = eng->em.count;

  // Get player position
  Vector3 *playerPos =
      getComponent(&eng->actors, gs->playerId, gs->compReg.cid_Positions);
  if (!playerPos)
    return;

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

    // Update the aim target component
    *aimTarget = calculatedTarget;

    // DEBUG: Print the aim target
    // printf("Tank %d aiming at: (%.2f, %.2f, %.2f)\n", i,
    //        aimTarget->x, aimTarget->y, aimTarget->z);
  }
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

void TestBarrelOrientation(Engine_t *eng, int tankId) {
  ModelCollection_t *mc = &eng->actors.modelCollections[tankId];

  printf("\n=== Testing Barrel Orientation ===\n");

  // Test 1: Set barrel to 0° (horizontal/forward)
  mc->localRotationOffset[2].pitch = 0;
  printf("Test 1: Barrel pitch = 0° (horizontal forward)\n");
  // Observe what direction barrel points

  // Wait a frame or two, then...

  // Test 2: Set barrel to 90° (vertical up)
  mc->localRotationOffset[2].pitch = PI / 2;
  printf("Test 2: Barrel pitch = 90° (vertical up)\n");

  // Test 3: Set barrel to 180° (horizontal backward)
  mc->localRotationOffset[2].pitch = PI;
  printf("Test 3: Barrel pitch = 180° (horizontal backward)\n");
}

void UpdateTankTurretAiming(GameState_t *gs, Engine_t *eng, float dt) {
  for (int i = 0; i < eng->em.count; i++) {
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

    // CLAMP PITCH to reasonable limits
    float maxPitchUp = PI * 0.3f;    // ~54 degrees up
    float maxPitchDown = PI * 0.15f; // ~27 degrees down

    if (targetPitch > maxPitchUp)
      targetPitch = maxPitchUp;
    if (targetPitch < -maxPitchDown)
      targetPitch = -maxPitchDown;

    // Smooth interpolation for nicer aiming
    float rotationSpeed = 0.5f; // radians per second
    float maxRotation = rotationSpeed * dt;

    // --- TURRET (model 1): Yaw rotation only ---
    // For turret, we want independent yaw rotation (not locked to parent)
    // Based on your transformation code, we should set localRotationOffset.yaw

    float currentTurretYaw = mc->localRotationOffset[1].yaw;
    float yawDiff = targetYaw - currentTurretYaw;

    // Handle wrap-around (angles go from -PI to PI)
    while (yawDiff > PI)
      yawDiff -= 2 * PI;
    while (yawDiff < -PI)
      yawDiff += 2 * PI;

    // Limit rotation speed
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

          // Calculate final world-space barrel direction
          // This is complex due to parent-child transforms
          // For now, use the calculated direction to target
          Vector3 barrelWorldPos =
              mc->globalPositions[2]; // Will be set by
                                      // UpdateModelCollectionWorldTransforms
          raycast->ray.position = barrelWorldPos;
          raycast->ray.direction = Vector3Normalize(direction);

          // Update ray orientation
          raycast->oriOffset.yaw = targetYaw;
          raycast->oriOffset.pitch = targetPitch;
          raycast->oriOffset.roll = 0;

          raycast->active = true;
          break;
        }
      }
    }

    // Debug output - less frequent
    static int debugCounter = 0;
    debugCounter++;
    if (debugCounter % 30 == 0) { // Print every ~3 seconds at 60fps
      printf("\n=== Tank %d Aiming ===\n", i);
      printf("Position: (%.1f, %.1f, %.1f)\n", tankPos->x, tankPos->y,
             tankPos->z);
      printf("Target: (%.1f, %.1f, %.1f)\n", aimTarget->x, aimTarget->y,
             aimTarget->z);
      printf("Distance: %.1f\n", Vector3Distance(*tankPos, *aimTarget));

      printf("Turret localRotationOffset.yaw: %.1f°\n",
             mc->localRotationOffset[1].yaw * RAD2DEG);
      printf("Barrel localRotationOffset.pitch: %.1f° (from vertical)\n",
             mc->localRotationOffset[2].pitch * RAD2DEG);
      printf("Target Pitch (from horizontal): %.1f°\n", targetPitch * RAD2DEG);
      printf("Calculated: %.1f° - %.1f° = %.1f°\n", (PI / 2) * RAD2DEG,
             targetPitch * RAD2DEG, ((PI / 2) - targetPitch) * RAD2DEG);
    }
  }
}
