#include "raylib.h"
#include "raymath.h"
#include "systems.h"
#include <math.h>
#include <stdlib.h>

static float tankAiAccum = 0.0f;

#define TANK_AI_HZ 15.0f
#define TANK_AI_DT (1.0f / TANK_AI_HZ)

typedef enum {
  AIRH_B1 = 1, // fly to random point near player
  AIRH_B2 = 2  // pause/aim/wait (shooting later)
} HarasserAIState;

#define AIRH_MIN_RADIUS 600.0f
#define AIRH_MAX_RADIUS 1200.0f
#define AIRH_REACHED_DIST 80.0f
#define AIRH_B2_DELAY 0.5f
#define AIRH_B2_COOLDOWN 0.5f

#define AIRH_BURST_SHOTS 6
#define AIRH_BURST_SPACING 0.12f
#define AIRH_MOVE_SPEED 500.0f
#define AIRH_FLY_HEIGHT 200.0f

#define AIRH_REACHED_DIST 80.0f

#define AIRH_B2_WAIT 2.0f // waits in B2 before going back to B1 (shoot later)

typedef enum {
  ALPHA_SENTRY = 1,
  ALPHA_DASH = 2,
} AlphaAIState;

#define ALPHA_SENTRY_TIME 3.0f
#define ALPHA_DASH_TIME 1.2f

#define ALPHA_DASH_MIN_R 250.0f
#define ALPHA_DASH_MAX_R 600.0f
#define ALPHA_DASH_SPEED_MULT 2.4f // faster than normal tank

static float Rand01(void) { return (float)rand() / (float)RAND_MAX; }

static Vector3 GetRandomPointNearPlayer(Vector3 playerPos, float minR,
                                        float maxR) {
  float a = Rand01() * 2.0f * PI;
  float r = minR + Rand01() * (maxR - minR);
  return (Vector3){playerPos.x + cosf(a) * r, 0.0f, playerPos.z + sinf(a) * r};
}

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

static void UpdateTankTargetsForEntity(GameState_t *gs, Engine_t *eng, int i,
                                       Vector3 playerPos, float dt) {
  uint32_t mask = eng->em.masks[i];
  if (!(mask & C_TANK_MOVEMENT))
    return;

  Vector3 *pos = getComponent(&eng->actors, i, gs->compReg.cid_Positions);
  Vector3 *moveTarget =
      getComponent(&eng->actors, i, gs->compReg.cid_moveTarget);
  int *state =
      (int *)getComponent(&eng->actors, i, gs->compReg.cid_moveBehaviour);
  float *stateTimer =
      (float *)getComponent(&eng->actors, i, gs->compReg.cid_moveTimer);

  if (!pos || !moveTarget || !state || !stateTimer)
    return;

  bool detected = PlayerInDetectionZone(playerPos);

  if (!detected) {
    *state = TANK_IDLE;
  } else {
    if (*state == TANK_IDLE) {
      *state = TANK_ALERT_CIRCLE;
      *stateTimer = CHARGE_COOLDOWN;
    }
  }

  switch ((TankAIState)(*state)) {
  case TANK_IDLE: {
    Vector3 idle = IDLE_POINT;
    idle.y = 0.0f;
    *moveTarget = idle;
    *stateTimer = 0.0f;
  } break;

  case TANK_ALERT_CIRCLE: {
    *moveTarget = GetCirclePointAroundPlayer(*pos, playerPos, CIRCLE_RADIUS);

    *stateTimer -= dt;
    if (*stateTimer <= 0.0f) {
      *state = TANK_ALERT_CHARGE;
      *stateTimer = CHARGE_DURATION;
    }
  } break;

  case TANK_ALERT_CHARGE: {
    *moveTarget = GetPointTowardsPlayer(*pos, playerPos, 1000.0f);

    *stateTimer -= dt;
    if (*stateTimer <= 0.0f) {
      *state = TANK_ALERT_CIRCLE;
      *stateTimer = CHARGE_COOLDOWN;
    }
  } break;
  }
}

static void UpdateHarasserTargetsForEntity(GameState_t *gs, Engine_t *eng,
                                           int i, Vector3 playerPos, float dt) {
  uint32_t mask = eng->em.masks[i];
  if (!(mask & C_AIRHARASSER_MOVEMENT))
    return;

  Vector3 *pos = getComponent(&eng->actors, i, gs->compReg.cid_Positions);
  Vector3 *moveTarget =
      getComponent(&eng->actors, i, gs->compReg.cid_moveTarget);
  Vector3 *aimTarget = getComponent(&eng->actors, i, gs->compReg.cid_aimTarget);
  int *state =
      (int *)getComponent(&eng->actors, i, gs->compReg.cid_moveBehaviour);
  float *stateTimer =
      (float *)getComponent(&eng->actors, i, gs->compReg.cid_moveTimer);

  if (!pos || !moveTarget || !aimTarget || !state || !stateTimer)
    return;

  // Ensure we stay at flight height (your actual movement system can also
  // enforce this)
  pos->y = AIRH_FLY_HEIGHT + playerPos.y;

  // Initialize state if needed
  if (*state != AIRH_B1 && *state != AIRH_B2) {
    *state = AIRH_B1;
    *stateTimer = 0.0f;
  }

  switch ((HarasserAIState)(*state)) {
  case AIRH_B1: {
    bool targetUnset =
        (fabsf(moveTarget->x) < 0.001f && fabsf(moveTarget->z) < 0.001f);

    float d = DistXZ(*pos, *moveTarget);

    if (targetUnset) {
      Vector3 t =
          GetRandomPointNearPlayer(playerPos, AIRH_MIN_RADIUS, AIRH_MAX_RADIUS);
      t.y = AIRH_FLY_HEIGHT;
      *moveTarget = t;
      *aimTarget = *moveTarget; // B1 aims at target
      break;
    }

    // Arrived -> go to B2
    if (d <= AIRH_REACHED_DIST) {
      *state = AIRH_B2;
      *stateTimer = AIRH_B2_WAIT; // or AIRH_B2_DELAY
      // park at current position during B2
      *moveTarget = (Vector3){pos->x, AIRH_FLY_HEIGHT, pos->z};
      *aimTarget = playerPos; // B2 aims at player
      break;
    }

    // In-flight: aim at move target
    *aimTarget = *moveTarget;
  } break;

  case AIRH_B2: {
    *aimTarget = playerPos;
    *moveTarget =
        (Vector3){pos->x, AIRH_FLY_HEIGHT + playerPos.y, pos->z}; // park

    *stateTimer -= dt;
    if (*stateTimer <= 0.0f) {
      *state = AIRH_B1;
      *stateTimer = 0.0f;
    }
  } break;
  }

  // printf("H state=%d pos=(%.1f %.1f %.1f) target=(%.1f %.1f %.1f)\n", *state,
  //        pos->x, pos->y, pos->z, moveTarget->x, moveTarget->y,
  //        moveTarget->z);
}

static void UpdateAlphaTankTargetsForEntity(GameState_t *gs, Engine_t *eng,
                                            int i, Vector3 playerPos,
                                            float dt) {
  uint32_t mask = eng->em.masks[i];
  if (!(mask & C_TANK_MOVEMENT))
    return;

  Vector3 *pos = getComponent(&eng->actors, i, gs->compReg.cid_Positions);
  Vector3 *moveTarget =
      getComponent(&eng->actors, i, gs->compReg.cid_moveTarget);
  Vector3 *aimTarget = getComponent(&eng->actors, i, gs->compReg.cid_aimTarget);
  int *state =
      (int *)getComponent(&eng->actors, i, gs->compReg.cid_moveBehaviour);
  float *stateTimer =
      (float *)getComponent(&eng->actors, i, gs->compReg.cid_moveTimer);

  if (!pos || !moveTarget || !aimTarget || !state || !stateTimer)
    return;

  // Always aim at player (turret system uses aimTarget)
  *aimTarget = playerPos;

  // Initialize
  if (*state != ALPHA_SENTRY && *state != ALPHA_DASH) {
    *state = ALPHA_SENTRY;
    *stateTimer = ALPHA_SENTRY_TIME;
  }

  switch ((AlphaAIState)(*state)) {
  case ALPHA_SENTRY: {
    // stand still: keep target at current pos
    *moveTarget = (Vector3){pos->x, 0.0f, pos->z};

    *stateTimer -= dt;
    if (*stateTimer <= 0.0f) {
      *state = ALPHA_DASH;
      *stateTimer = ALPHA_DASH_TIME;

      Vector3 t = GetRandomPointNearPlayer(playerPos, ALPHA_DASH_MIN_R,
                                           ALPHA_DASH_MAX_R);
      t.y = 0.0f;
      *moveTarget = t;
    }
  } break;

  case ALPHA_DASH: {
    // keep moveTarget as the chosen dash point

    *stateTimer -= dt;
    if (*stateTimer <= 0.0f) {
      *state = ALPHA_SENTRY;
      *stateTimer = ALPHA_SENTRY_TIME;

      // stop moving
      *moveTarget = (Vector3){pos->x, 0.0f, pos->z};
    }
  } break;
  }
}

void UpdateEnemyTargets(GameState_t *gs, Engine_t *eng, SoundSystem_t *soundSys,
                        float dt) {
  tankAiAccum += dt;
  if (tankAiAccum < TANK_AI_DT)
    return;
  tankAiAccum -= TANK_AI_DT;

  int emCount = eng->em.count;
  Vector3 *playerPos =
      getComponent(&eng->actors, gs->playerId, gs->compReg.cid_Positions);
  if (!playerPos)
    return;

  for (int i = 0; i < emCount; i++) {
    if (!eng->em.alive[i])
      continue;

    switch (eng->actors.types[i]) {
    case ENTITY_TANK:
      UpdateTankTargetsForEntity(gs, eng, i, *playerPos, dt);
      break;

    case ENTITY_HARASSER:
      // printf("updating harasser targets\n");
      UpdateHarasserTargetsForEntity(gs, eng, i, *playerPos, dt);
      break;

    case ENTITY_TANK_ALPHA:
      UpdateAlphaTankTargetsForEntity(gs, eng, i, *playerPos, dt);
      break;

    default:
      break;
    }
  }
}

static void UpdateTankVelocityForEntity(GameState_t *gs, Engine_t *eng, int i,
                                        float dt) {
  uint32_t mask = eng->em.masks[i];
  if (!(mask & C_TANK_MOVEMENT))
    return;

  Vector3 *position = getComponent(&eng->actors, i, gs->compReg.cid_Positions);
  Vector3 *velocity = getComponent(&eng->actors, i, gs->compReg.cid_velocities);
  Vector3 *moveTarget =
      getComponent(&eng->actors, i, gs->compReg.cid_moveTarget);
  int *moveBehaviour =
      (int *)getComponent(&eng->actors, i, gs->compReg.cid_moveBehaviour);
  float moveSpeed = 50;

  if (!position || !velocity || !moveTarget || !moveBehaviour)
    return;

  position->y = GetTerrainHeightAtXZ(&gs->terrain, position->x, position->z);

  Vector3 direction = {moveTarget->x - position->x, 0.0f,
                       moveTarget->z - position->z};

  ModelCollection_t *mc = &eng->actors.modelCollections[i];

  float targetYaw = atan2f(direction.x, direction.z);
  float currentYaw = mc->localRotationOffset[0].yaw;

  float turnSpeed = 5.0f;
  float maxTurn = turnSpeed * dt;

  float yawDiff = targetYaw - currentYaw;
  while (yawDiff > PI)
    yawDiff -= 2.0f * PI;
  while (yawDiff < -PI)
    yawDiff += 2.0f * PI;

  if (yawDiff > maxTurn)
    yawDiff = maxTurn;
  if (yawDiff < -maxTurn)
    yawDiff = -maxTurn;

  mc->localRotationOffset[0].yaw = currentYaw + yawDiff;

  float distanceSquared = direction.x * direction.x + direction.z * direction.z;

  float adjustedMoveSpeed = moveSpeed;
  if (*moveBehaviour == TANK_ALERT_CHARGE)
    adjustedMoveSpeed = moveSpeed * 1.5f;

  if (distanceSquared > 1.0f) {
    float distance = sqrtf(distanceSquared);
    direction.x /= distance;
    direction.z /= distance;

    velocity->x = direction.x * adjustedMoveSpeed;
    velocity->z = direction.z * adjustedMoveSpeed;
  } else {
    velocity->x = 0.0f;
    velocity->z = 0.0f;
  }
  velocity->y = 0.0f;
}

#define AIRH_TURN_SPEED 6.5f

static void UpdateHarasserVelocityForEntity(GameState_t *gs, Engine_t *eng,
                                            int i, Vector3 playerPos,
                                            float dt) {
  uint32_t mask = eng->em.masks[i];
  if (!(mask & C_AIRHARASSER_MOVEMENT))
    return;

  Vector3 *position = getComponent(&eng->actors, i, gs->compReg.cid_Positions);
  Vector3 *velocity = getComponent(&eng->actors, i, gs->compReg.cid_velocities);
  Vector3 *moveTarget =
      getComponent(&eng->actors, i, gs->compReg.cid_moveTarget);
  int *state =
      (int *)getComponent(&eng->actors, i, gs->compReg.cid_moveBehaviour);

  if (!position || !velocity || !moveTarget || !state)
    return;

  // Keep flight height (pick one convention and use it everywhere)
  position->y = AIRH_FLY_HEIGHT + playerPos.y;

  ModelCollection_t *mc = &eng->actors.modelCollections[i];

  // Choose what to face based on state
  Vector3 facePoint = (*state == AIRH_B2) ? playerPos : *moveTarget;
  facePoint.y += 12;
  int rayIdx = 0;

  Raycast_t *rc = &eng->actors.raycasts[i][rayIdx];
  rc->ray.position = *position;
  if (rc->parentModelIndex == 1) {
    Vector3 dir =
        Vector3Normalize(Vector3Subtract(facePoint, rc->ray.position));
    rc->ray.direction = dir;
    rc->active = true;
  }

  // --- AIM (same style as tank) ---
  Vector3 aimDir = {facePoint.x - position->x, 0.0f, facePoint.z - position->z};

  float aimLen2 = aimDir.x * aimDir.x + aimDir.z * aimDir.z;
  if (aimLen2 > 0.0001f) {
    float targetYaw = atan2f(aimDir.x, aimDir.z);
    float currentYaw = mc->localRotationOffset[0].yaw;

    float turnSpeed = AIRH_TURN_SPEED; // e.g. 6.5f
    float maxTurn = turnSpeed * dt;

    float yawDiff = targetYaw - currentYaw;
    while (yawDiff > PI)
      yawDiff -= 2.0f * PI;
    while (yawDiff < -PI)
      yawDiff += 2.0f * PI;

    if (yawDiff > maxTurn)
      yawDiff = maxTurn;
    if (yawDiff < -maxTurn)
      yawDiff = -maxTurn;

    mc->localRotationOffset[0].yaw = currentYaw + yawDiff;

    // If your renderer uses orientations instead, uncomment:
    // mc->orientations[0].yaw = mc->localRotationOffset[0].yaw;
  }

  // --- MOVE ---
  if (*state == AIRH_B2) {
    // Pause while aiming at player (shooting handled elsewhere)
    velocity->x = 0.0f;
    velocity->y = 0.0f;
    velocity->z = 0.0f;
    return;
  }

  // Behaviour 1: move toward moveTarget
  Vector3 moveDir = {moveTarget->x - position->x, 0.0f,
                     moveTarget->z - position->z};
  float d2 = moveDir.x * moveDir.x + moveDir.z * moveDir.z;

  if (d2 > 1.0f) {
    float d = sqrtf(d2);
    moveDir.x /= d;
    moveDir.z /= d;

    velocity->x = moveDir.x * AIRH_MOVE_SPEED; // e.g. 140.0f
    velocity->y = 0.0f;
    velocity->z = moveDir.z * AIRH_MOVE_SPEED;
  } else {
    velocity->x = 0.0f;
    velocity->y = 0.0f;
    velocity->z = 0.0f;
  }
}

static void UpdateAlphaTankVelocityForEntity(GameState_t *gs, Engine_t *eng,
                                             int i, float dt) {
  uint32_t mask = eng->em.masks[i];
  if (!(mask & C_TANK_MOVEMENT))
    return;

  Vector3 *position = getComponent(&eng->actors, i, gs->compReg.cid_Positions);
  Vector3 *velocity = getComponent(&eng->actors, i, gs->compReg.cid_velocities);
  Vector3 *moveTarget =
      getComponent(&eng->actors, i, gs->compReg.cid_moveTarget);
  int *state =
      (int *)getComponent(&eng->actors, i, gs->compReg.cid_moveBehaviour);

  if (!position || !velocity || !moveTarget || !state)
    return;

  position->y = GetTerrainHeightAtXZ(&gs->terrain, position->x, position->z);

  // Stop in sentry mode
  if (*state == ALPHA_SENTRY) {
    velocity->x = 0.0f;
    velocity->y = 0.0f;
    velocity->z = 0.0f;
    return;
  }

  // Dash mode movement (tank-like)
  Vector3 direction = {moveTarget->x - position->x, 0.0f,
                       moveTarget->z - position->z};

  ModelCollection_t *mc = &eng->actors.modelCollections[i];

  float targetYaw = atan2f(direction.x, direction.z);
  float currentYaw = mc->localRotationOffset[0].yaw;

  float turnSpeed = 7.0f; // alpha turns faster than normal tank
  float maxTurn = turnSpeed * dt;

  float yawDiff = targetYaw - currentYaw;
  while (yawDiff > PI)
    yawDiff -= 2.0f * PI;
  while (yawDiff < -PI)
    yawDiff += 2.0f * PI;

  if (yawDiff > maxTurn)
    yawDiff = maxTurn;
  if (yawDiff < -maxTurn)
    yawDiff = -maxTurn;

  mc->localRotationOffset[0].yaw = currentYaw + yawDiff;

  float dist2 = direction.x * direction.x + direction.z * direction.z;
  if (dist2 > 1.0f) {
    float dist = sqrtf(dist2);
    direction.x /= dist;
    direction.z /= dist;

    float baseSpeed = 50.0f; // same as tank
    float speed = baseSpeed * ALPHA_DASH_SPEED_MULT;

    velocity->x = direction.x * speed;
    velocity->y = 0.0f;
    velocity->z = direction.z * speed;
  } else {
    velocity->x = 0.0f;
    velocity->y = 0.0f;
    velocity->z = 0.0f;
  }
}

void UpdateEnemyVelocities(GameState_t *gs, Engine_t *eng,
                           SoundSystem_t *soundSys, float dt) {
  int emCount = eng->em.count;

  Vector3 *playerPos =
      getComponent(&eng->actors, gs->playerId, gs->compReg.cid_Positions);

  for (int i = 0; i < emCount; i++) {
    if (!eng->em.alive[i])
      continue;

    switch (eng->actors.types[i]) {
    case ENTITY_TANK:
      UpdateTankVelocityForEntity(gs, eng, i, dt);
      break;

    case ENTITY_HARASSER:
      if (playerPos)
        UpdateHarasserVelocityForEntity(gs, eng, i, *playerPos, dt);
      break;

    case ENTITY_TANK_ALPHA:
      UpdateAlphaTankVelocityForEntity(gs, eng, i, dt);
      break;

    case ENTITY_MECH:
    case ENTITY_TURRET:
    default:
      break;
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

static float Rand01f(void) {
  return (float)GetRandomValue(0, 10000) / 10000.0f;
}
static float RandSigned1f(void) { return Rand01f() * 2.0f - 1.0f; }

// Rotate a direction by yaw (around Y) and pitch (around X in local space)
static Vector3 ApplyYawPitchToDir(Vector3 dir, float yaw, float pitch) {
  // yaw around Y
  float cy = cosf(yaw), sy = sinf(yaw);
  Vector3 d1 =
      (Vector3){dir.x * cy + dir.z * sy, dir.y, -dir.x * sy + dir.z * cy};

  // pitch around X
  float cp = cosf(pitch), sp = sinf(pitch);
  Vector3 d2 = (Vector3){d1.x, d1.y * cp - d1.z * sp, d1.y * sp + d1.z * cp};

  return Vector3Normalize(d2);
}

void UpdateHarasserAimingAndShooting(GameState_t *gs, Engine_t *eng,
                                     SoundSystem_t *soundSys, float dt) {
  int emCount = eng->em.count;

  Vector3 *playerPos =
      getComponent(&eng->actors, gs->playerId, gs->compReg.cid_Positions);
  if (!playerPos)
    return;

  // per-entity burst state
  static int phase[MAX_ENTITIES] = {
      0}; // 0=idle/not in B2, 1=prewait, 2=bursting, 3=postwait
  static float phaseT[MAX_ENTITIES] = {0};
  static int shotsLeft[MAX_ENTITIES] = {0};
  static float spacingT[MAX_ENTITIES] = {0};
  static int prevState[MAX_ENTITIES] = {0}; // to detect entering B2

  for (int i = 0; i < emCount; i++) {
    if (!eng->em.alive[i])
      continue;
    if (eng->actors.types[i] != ENTITY_HARASSER)
      continue;

    uint32_t mask = eng->em.masks[i];
    if (!(mask & C_RAYCAST) || !(mask & C_COOLDOWN_TAG))
      continue;

    int *state =
        (int *)getComponent(&eng->actors, i, gs->compReg.cid_moveBehaviour);
    if (!state)
      continue;

    // Detect entering/leaving B2
    if (*state != AIRH_B2) {
      phase[i] = 0;
      phaseT[i] = 0.0f;
      shotsLeft[i] = 0;
      spacingT[i] = 0.0f;
      prevState[i] = *state;
      continue;
    }

    // Entered B2 this frame -> arm sequence
    if (prevState[i] != AIRH_B2) {
      phase[i] = 1; // prewait
      phaseT[i] = AIRH_B2_DELAY;
      shotsLeft[i] = AIRH_BURST_SHOTS;
      spacingT[i] = 0.0f;
    }
    prevState[i] = *state;

    // Find the gun ray (parentModelIndex == 1 for harasser)
    int gunRayIdx = -1;
    for (int rayIdx = 0; rayIdx < eng->actors.rayCounts[i]; rayIdx++) {
      Raycast_t *rc = &eng->actors.raycasts[i][rayIdx];
      if (rc->active && rc->parentModelIndex == 1) {
        gunRayIdx = rayIdx;
        break;
      }
    }
    if (gunRayIdx < 0)
      continue;

    // Cooldown gating (you can also remove this entirely since we have
    // spacingT)
    float *cooldown = eng->actors.cooldowns[i];
    if (!cooldown)
      continue;

    // Keep cooldown ticking down
    if (*cooldown > 0.0f) {
      *cooldown -= dt;
      continue;
    }

    // ---- Phase machine ----

    // Phase 1: pre-burst waiting
    if (phase[i] == 1) {
      phaseT[i] -= dt;
      if (phaseT[i] > 0.0f) {
        *cooldown = 0.01f;
        continue;
      }
      phase[i] = 2;       // start bursting
      spacingT[i] = 0.0f; // allow immediate first shot
    }

    // Phase 2: bursting
    if (phase[i] == 2) {
      if (shotsLeft[i] <= 0) {
        // burst finished -> post wait
        phase[i] = 3;
        phaseT[i] = AIRH_B2_COOLDOWN;
        *cooldown = 0.01f;
        continue;
      }

      spacingT[i] -= dt;
      if (spacingT[i] > 0.0f) {
        *cooldown = 0.01f;
        continue;
      }

      // FIRE one bullet
      Raycast_t *rc = &eng->actors.raycasts[i][gunRayIdx];

      Vector3 toPlayer = Vector3Subtract(*playerPos, rc->ray.position);
      float dist = sqrtf(toPlayer.x * toPlayer.x + toPlayer.y * toPlayer.y +
                         toPlayer.z * toPlayer.z);
      if (dist < 0.001f)
        dist = 0.001f;

      // Perfect aim direction (used for gating)
      Vector3 perfectDir = Vector3Scale(toPlayer, 1.0f / dist);

      // Gate using perfect aim so spread doesn’t prevent firing
      if (!BarrelAimingAtPlayer(rc->ray.position, perfectDir, *playerPos,
                                12.0f)) {
        *cooldown = 0.01f;
        continue;
      }

      // --- Apply slight spread for the actual shot ---
      // Base spread in radians (e.g. ~0.75 degrees)
      float baseSpread = 0.005f;

      // Optional: increase a bit with distance (clamped)
      float distFactor = dist / 800.0f; // tune denominator
      if (distFactor > 1.0f)
        distFactor = 1.0f;
      float spread = baseSpread * (1.0f + 0.8f * distFactor);

      // Random yaw/pitch offsets
      float yawErr = RandSigned1f() * spread;
      float pitchErr = RandSigned1f() * spread;

      // Final shot direction
      Vector3 shotDir = ApplyYawPitchToDir(perfectDir, yawErr, pitchErr);

      // Use spread direction for projectile + effects
      rc->ray.direction = shotDir;

      Vector3 shooterPos =
          *(Vector3 *)getComponent(&eng->actors, i, gs->compReg.cid_Positions);
      QueueSound(soundSys, SOUND_WEAPON_FIRE, shooterPos, 0.2f, 1.0f);

      FireProjectile(eng, (entity_t)i, gunRayIdx, 0, 1);

      const float muzzleOffset = 2.0f;
      Vector3 muzzlePos = Vector3Add(
          rc->ray.position, Vector3Scale(rc->ray.direction, muzzleOffset));
      SpawnSmoke(eng, muzzlePos);

      shotsLeft[i]--;
      spacingT[i] = AIRH_BURST_SPACING;
      *cooldown = 0.01f;
      continue;
    }

    // Phase 3: post-burst waiting -> switch back to B1
    if (phase[i] == 3) {
      phaseT[i] -= dt;
      if (phaseT[i] > 0.0f) {
        *cooldown = 0.01f;
        continue;
      }

      // ✅ Done: go back to behaviour 1
      *state = AIRH_B1;
      phase[i] = 0;
      phaseT[i] = 0.0f;
      shotsLeft[i] = 0;
      spacingT[i] = 0.0f;

      // invalidate target so B1 chooses a new point
      Vector3 *moveTarget =
          getComponent(&eng->actors, i, gs->compReg.cid_moveTarget);
      if (moveTarget)
        *moveTarget = (Vector3){0, AIRH_FLY_HEIGHT, 0};

      *cooldown = 0.01f;
      continue;
    }
  }
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
    QueueSound(soundSys, SOUND_WEAPON_FIRE, shooterPos, 0.2f, 1.0f);

    FireProjectile(eng, (entity_t)i, barrelRayIdx, 0, 1);

    // Smoke at muzzle: start at barrel ray origin, move a bit forward along
    // barrel dir
    const float muzzleOffset = 2.0f; // tweak to match your barrel length
    Vector3 muzzlePos =
        Vector3Add(ray.position, Vector3Scale(ray.direction, muzzleOffset));
    SpawnSmoke(eng, muzzlePos);

    // Reset cooldown from firerate (shots per second). If missing, default 1
    // shot/sec.
    *cooldown = *firerate;
  }
}

static inline float ClampF(float v, float lo, float hi) {
  return (v < lo) ? lo : (v > hi) ? hi : v;
}

void UpdateAlphaTankTurretAimingAndShooting(GameState_t *gs, Engine_t *eng,
                                            SoundSystem_t *soundSys, float dt) {
  int emCount = eng->em.count;

  Vector3 *playerPos =
      getComponent(&eng->actors, gs->playerId, gs->compReg.cid_Positions);
  if (!playerPos)
    return;

  // -----------------------------
  // PASS 1: turret + barrel aiming + ray update
  // -----------------------------
  for (int i = 0; i < emCount; i++) {
    if (!eng->em.alive[i])
      continue;
    if (eng->actors.types[i] != ENTITY_TANK_ALPHA)
      continue;

    Vector3 *tankPos = getComponent(&eng->actors, i, gs->compReg.cid_Positions);
    Vector3 *aimTarget =
        getComponent(&eng->actors, i, gs->compReg.cid_aimTarget);
    if (!tankPos || !aimTarget)
      continue;

    ModelCollection_t *mc = &eng->actors.modelCollections[i];
    if (!mc || mc->countModels < 3)
      continue;
    if (!mc->localRotationOffset)
      continue;

    // direction from tank to aim target
    Vector3 direction = Vector3Subtract(*aimTarget, *tankPos);

    float horizontalDist =
        sqrtf(direction.x * direction.x + direction.z * direction.z);
    float targetPitch = -atan2f(direction.y, horizontalDist);

    float rotationSpeed = 2.5f;
    float maxRotation = rotationSpeed * dt;

    // --- TURRET yaw ---
    float baseYaw = mc->localRotationOffset[0].yaw;
    float targetYawWorld = atan2f(direction.x, direction.z);
    float targetYawLocal = targetYawWorld - baseYaw;

    while (targetYawLocal > PI)
      targetYawLocal -= 2.0f * PI;
    while (targetYawLocal < -PI)
      targetYawLocal += 2.0f * PI;

    float currentTurretYaw = mc->localRotationOffset[1].yaw;
    float yawDiff = targetYawLocal - currentTurretYaw;
    while (yawDiff > PI)
      yawDiff -= 2.0f * PI;
    while (yawDiff < -PI)
      yawDiff += 2.0f * PI;

    yawDiff = ClampF(yawDiff, -maxRotation, maxRotation);
    mc->localRotationOffset[1].yaw = currentTurretYaw + yawDiff;
    mc->localRotationOffset[1].pitch = 0.0f;
    mc->localRotationOffset[1].roll = 0.0f;

    // --- BARREL pitch ---
    float currentBarrelPitch = mc->localRotationOffset[2].pitch;
    float targetBarrelPitch = -targetPitch;

    float pitchDiff = targetBarrelPitch - currentBarrelPitch;
    pitchDiff = ClampF(pitchDiff, -maxRotation, maxRotation);
    mc->localRotationOffset[2].pitch = currentBarrelPitch + pitchDiff;
    mc->localRotationOffset[2].yaw = 0.0f;
    mc->localRotationOffset[2].roll = 0.0f;

    // sync orientations if used elsewhere
    if (mc->orientations) {
      mc->orientations[1].yaw = mc->localRotationOffset[1].yaw;
      mc->orientations[1].pitch = 0.0f;
      mc->orientations[1].roll = 0.0f;

      mc->orientations[2].yaw = 0.0f;
      mc->orientations[2].pitch = mc->localRotationOffset[2].pitch;
      mc->orientations[2].roll = 0.0f;
    }

    // Update barrel ray (parentModelIndex == 2)
    if (eng->actors.rayCounts[i] > 0) {
      for (int rayIdx = 0; rayIdx < eng->actors.rayCounts[i]; rayIdx++) {
        Raycast_t *raycast = &eng->actors.raycasts[i][rayIdx];
        if (raycast->parentModelIndex != 2)
          continue;

        Vector3 aimPos = *aimTarget;
        Vector3 muzzlePos = mc->globalPositions[2];

        // Weapon params: use gun 0 ballistics for aiming compensation
        int gunId = 0;
        float muzzleVel = (eng->actors.muzzleVelocities[i] &&
                           eng->actors.muzzleVelocities[i][gunId] > 0.0f)
                              ? eng->actors.muzzleVelocities[i][gunId]
                              : 10.0f;
        float dropRate =
            (eng->actors.dropRates[i] && eng->actors.dropRates[i][gunId] > 0.0f)
                ? eng->actors.dropRates[i][gunId]
                : 1.0f;

        Vector3 toTarget = Vector3Subtract(aimPos, muzzlePos);
        float dxz = sqrtf(toTarget.x * toTarget.x + toTarget.z * toTarget.z);
        if (dxz < 0.001f)
          dxz = 0.001f;

        float t = dxz / muzzleVel;
        float drop = 1.2f * dropRate * t * t;
        aimPos.y += drop;

        Vector3 dirToAim = Vector3Normalize(Vector3Subtract(aimPos, muzzlePos));

        float turretYawWorld2 = atan2f(dirToAim.x, dirToAim.z);
        float pitchFromHorizontal = asinf(dirToAim.y);

        Vector3 barrelDirWorld =
            ForwardFromYawPitch(turretYawWorld2, pitchFromHorizontal);

        raycast->ray.position = muzzlePos;
        raycast->ray.direction = barrelDirWorld;
        raycast->active = true;
        break;
      }
    }
  }

  // -----------------------------
  // PASS 2: shooting (bullets in sentry, missiles in dash)
  // -----------------------------
  for (int i = 0; i < emCount; i++) {
    if (!eng->em.alive[i])
      continue;
    if (eng->actors.types[i] != ENTITY_TANK_ALPHA)
      continue;

    uint32_t mask = eng->em.masks[i];
    if (!(mask & C_TURRET_BEHAVIOUR_1))
      continue;

    int *state =
        (int *)getComponent(&eng->actors, i, gs->compReg.cid_moveBehaviour);
    if (!state)
      continue;

    // Find barrel ray index (parentModelIndex == 2)
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

    // Need 2-weapon arrays
    float *cooldowns = eng->actors.cooldowns[i];
    float *firerate = eng->actors.firerate[i];
    if (!cooldowns || !firerate)
      continue;

    // Choose weapon based on alpha state
    int gunId;
    int projType;

    if (*state == ALPHA_DASH) {
      gunId = 1;
      projType = P_MISSILE; // homing missile
    } else {
      gunId = 0;
      projType = P_BULLET;
    }

    // Tick this weapon's cooldown
    if (cooldowns[gunId] > 0.0f) {
      cooldowns[gunId] -= dt;
      continue;
    }

    // Gate firing:
    // - bullets: require aiming at player
    // - missiles: allow more lenient (since they home anyway)
    if (projType == P_BULLET) {
      if (!BarrelAimingAtPlayer(ray.position, ray.direction, *playerPos, 10.0f))
        continue;
    } else {
      // optional mild gate so missiles don't fire backwards
      if (!BarrelAimingAtPlayer(ray.position, ray.direction, *playerPos, 35.0f))
        continue;
    }

    // FIRE
    Vector3 shooterPos =
        *(Vector3 *)getComponent(&eng->actors, i, gs->compReg.cid_Positions);
    SoundType_t soundType =
        projType == P_BULLET ? SOUND_WEAPON_FIRE : SOUND_ROCKET_FIRE;
    QueueSound(soundSys, soundType, shooterPos, 0.2f, 1.0f);

    FireProjectile(eng, (entity_t)i, barrelRayIdx, gunId, projType);

    const float muzzleOffset = 50;
    Vector3 muzzlePos =
        Vector3Add(ray.position, Vector3Scale(ray.direction, muzzleOffset));
    SpawnSmoke(eng, muzzlePos);

    // Reset cooldown from firerate[gunId]
    cooldowns[gunId] = firerate[gunId];
  }
}
