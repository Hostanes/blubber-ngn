#include "systems.h"
#include <math.h>
#include <stdlib.h>

// Helper function to get a random point on a circle around a center position
Vector3 GetRandomPointAroundPosition(Vector3 center, float maxRadius) {
  Vector3 result;

  float angle =
      ((float)rand() / RAND_MAX) * 2.0f * 3.14159265f; // Random angle 0-2π
  float radius =
      ((float)rand() / RAND_MAX) * maxRadius; // Random radius 0-maxRadius

  float xOffset = cosf(angle) * radius;
  float zOffset = sinf(angle) * radius;

  result.x = center.x + xOffset;
  result.y = 0.0f; // Keep y = 0
  result.z = center.z + zOffset;

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

    uint32_t mask = eng->em.masks[i];
    if (mask & C_TANK_MOVEMENT) {
      float *timer =
          (float *)getComponent(&eng->actors, i, gs->compReg.cid_moveTimer);
      Vector3 *moveTarget =
          getComponent(&eng->actors, i, gs->compReg.cid_moveTarget);

      if (*timer <= 0) {
        *timer = 10.0f; // Reset timer

        // Get new random target position
        *moveTarget = GetRandomPointAroundPosition(*playerPos, 500.0f);
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
    // printf("checking masks\n");
    // Only update velocity for entities that can move
    if (mask & C_TANK_MOVEMENT) {
      // Get required components
      Vector3 *position =
          getComponent(&eng->actors, i, gs->compReg.cid_Positions);
      Vector3 *velocity =
          getComponent(&eng->actors, i, gs->compReg.cid_velocities);
      Vector3 *moveTarget =
          getComponent(&eng->actors, i, gs->compReg.cid_moveTarget);
      float moveSpeed = 100;

      if (!position || !velocity || !moveTarget || !moveSpeed)
        continue;

      // Calculate direction to target
      Vector3 direction;
      direction.x = moveTarget->x - position->x;
      direction.y = 0.0f; // We're only moving in horizontal plane
      direction.z = moveTarget->z - position->z;

      // Normalize the direction (if not zero)
      float distanceSquared =
          direction.x * direction.x + direction.z * direction.z;

      // Only move if we're not already at the target
      if (distanceSquared > 1.0f) { // Small threshold
        float distance = sqrtf(distanceSquared);
        direction.x /= distance;
        direction.z /= distance;

        velocity->x = direction.x * (moveSpeed);
        velocity->y = 0.0f;
        velocity->z = direction.z * (moveSpeed);
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
