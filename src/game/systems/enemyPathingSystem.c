#include "../game.h"
#include "enemy_behaviour.h"
#include "systems.h"

const float faceThreshold = 0.5f;
const float arriveThreshold = 0.5f;

void EnemyAISystem(world_t *world, GameWorld *game, archetype_t *enemyArch,
                   float dt) {
  for (uint32_t i = 0; i < enemyArch->count; i++) {
    entity_t e = enemyArch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value)
      continue;

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    Velocity *vel = ECS_GET(world, e, Velocity, COMP_VELOCITY);
    NavPath *path = ECS_GET(world, e, NavPath, COMP_NAVPATH);

    if (!path || path->count == 0)
      continue;

    if (path->currentIndex >= path->count) {
      vel->value.x = 0;
      vel->value.z = 0;
      continue;
    }
    pos->value.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                                 pos->value.x, pos->value.z);

    Vector3 target = path->points[path->currentIndex];

    Vector3 toTarget = Vector3Subtract(target, pos->value);
    toTarget.y = 0.0f;

    float distance = Vector3Length(toTarget);
    // printf("target = %f, %f\n:", toTarget.x, toTarget.z);

    // If close to waypoint -> advance
    if (distance < arriveThreshold) {
      path->currentIndex++;

      // If that was the final node -> STOP completely
      if (path->currentIndex >= path->count) {
        vel->value.x = 0;
        vel->value.z = 0;
        path->count = 0;        // clear path
        path->currentIndex = 0; // reset

        continue;
      }

      continue;
    }

    // Move toward waypoint
    if (distance > 0.001f) {

      Vector3 dir = Vector3Normalize(toTarget);
      float targetYaw = atan2f(dir.x, dir.z);

      Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);

      float delta = targetYaw - ori->yaw;

      while (delta > PI)
        delta -= 2 * PI;
      while (delta < -PI)
        delta += 2 * PI;

      float maxStep = rotateSpeeds[0] * dt;

      if (fabsf(delta) <= maxStep) {
        ori->yaw = targetYaw; // snap
      } else {
        ori->yaw += (delta > 0 ? 1 : -1) * maxStep;
      }

      if (fabsf(delta) < faceThreshold) {
        vel->value.x = dir.x * moveSpeeds[0];
        vel->value.z = dir.z * moveSpeeds[0];
      } else {
        vel->value.x = 0;
        vel->value.z = 0;
      }
    }
  }
}

void EnemyGruntAISystem(world_t *world, GameWorld *game, archetype_t *enemyArch,
                        float dt) {
  const float minDist = 10.0f;
  const float maxDist = 50.0f;
  const float repathInterval = 5.0f;

  Position *playerPos = ECS_GET(world, game->player, Position, COMP_POSITION);
  if (!playerPos)
    return;

  for (uint32_t i = 0; i < enemyArch->count; i++) {
    entity_t e = enemyArch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value)
      continue;

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    NavPath *path = ECS_GET(world, e, NavPath, COMP_NAVPATH);
    Timer *repathTimer = ECS_GET(world, e, Timer, COMP_MOVE_TIMER);
    CombatState_t *combat = ECS_GET(world, e, CombatState_t, COMP_COMBAT_STATE);

    if (!pos || !path || !repathTimer || !combat)
      continue;

    // 1. Update the local timer
    if (repathTimer->value > 0.0f) {
      repathTimer->value -= dt;
    }

    // 2. Calculate horizontal distance to player
    Vector3 toPlayer = Vector3Subtract(playerPos->value, pos->value);
    toPlayer.y = 0.0f;
    float distToPlayer = Vector3Length(toPlayer);

    // 3. State Management Logic
    switch (combat->state) {

    case ENEMY_STATE_MOVING: {
      // If we are currently moving, check if the generic EnemyAISystem
      // finished the path (it clears path->count when done).
      if (path->count == 0) {
        combat->state = ENEMY_STATE_COMBAT;
      }
      // While moving, we don't do anything else; let EnemyAISystem drive.
    } break;

    case ENEMY_STATE_COMBAT: {
      // If we are standing still (Combat), check if we've become out of range
      if (distToPlayer < minDist || distToPlayer > maxDist) {

        // Only try to repath if our cooldown timer is done
        if (repathTimer->value <= 0.0f) {
          bool foundPath = false;

          // Try to find a valid spot in the "sweet spot" ring around the player
          for (int attempt = 0; attempt < 10; attempt++) {
            float angle = GetRandomValue(0, 360) * DEG2RAD;
            float radius = GetRandomValue((int)minDist, (int)maxDist);

            Vector3 candidate = {playerPos->value.x + cosf(angle) * radius,
                                 0.0f,
                                 playerPos->value.z + sinf(angle) * radius};

            // Snap to terrain height
            candidate.y = HeightMap_GetHeightCatmullRom(
                &game->terrainHeightMap, candidate.x, candidate.z);

            // Validate grid cell
            int cx, cy;
            if (!NavGrid_WorldToCell(&game->navGrid, candidate, &cx, &cy))
              continue;
            int idx = NavGrid_Index(&game->navGrid, cx, cy);
            if (game->navGrid.cells[idx].type == NAV_CELL_WALL)
              continue;

            // Attempt pathfinding
            if (NavGrid_FindPath(&game->navGrid, pos->value, candidate, path)) {
              combat->state = ENEMY_STATE_MOVING;
              repathTimer->value = repathInterval;
              foundPath = true;
              break;
            }
          }

          // If all attempts failed, we stay in COMBAT state
          // and try again next time the timer allows.
        }
      }
    } break;

    default: {
      // Initial safety check
      combat->state = ENEMY_STATE_COMBAT;
    } break;
    }
  }
}

void EnemyRangerAISystem(world_t *world, GameWorld *game,
                         archetype_t *enemyArch, float dt) {
  const float minDist = 25.0f;       // larger inner ring
  const float maxDist = 90.0f;       // larger outer ring
  const float repathInterval = 8.0f; // moves less frequently

  Position *playerPos = ECS_GET(world, game->player, Position, COMP_POSITION);
  if (!playerPos)
    return;

  for (uint32_t i = 0; i < enemyArch->count; i++) {
    entity_t e = enemyArch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value)
      continue;

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    NavPath *path = ECS_GET(world, e, NavPath, COMP_NAVPATH);
    Timer *repathTimer = ECS_GET(world, e, Timer, COMP_MOVE_TIMER);
    CombatState_t *combat = ECS_GET(world, e, CombatState_t, COMP_COMBAT_STATE);

    if (!pos || !path || !repathTimer || !combat)
      continue;

    if (repathTimer->value > 0.0f)
      repathTimer->value -= dt;

    Vector3 toPlayer = Vector3Subtract(playerPos->value, pos->value);
    toPlayer.y = 0.0f;
    float distToPlayer = Vector3Length(toPlayer);

    switch (combat->state) {

    case ENEMY_STATE_MOVING:
      if (path->count == 0)
        combat->state = ENEMY_STATE_COMBAT;
      break;

    case ENEMY_STATE_COMBAT:

      if (distToPlayer < minDist || distToPlayer > maxDist) {

        if (repathTimer->value <= 0.0f) {

          Vector3 dir = Vector3Normalize(toPlayer);

          float desiredDist = (minDist + maxDist) * 0.5f;

          Vector3 ringTarget =
              Vector3Subtract(playerPos->value, Vector3Scale(dir, desiredDist));

          ringTarget.y = playerPos->value.y;

          if (NavGrid_FindPath(&game->navGrid, pos->value, ringTarget, path)) {

            combat->state = ENEMY_STATE_MOVING;
            repathTimer->value = repathInterval;
          }
        }
      }
      break;

    default:
      combat->state = ENEMY_STATE_COMBAT;
      break;
    }
  }
}
