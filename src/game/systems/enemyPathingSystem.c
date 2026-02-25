#include "../game.h"
#include "enemy_behaviour.h"
#include "systems.h"

const float faceThreshold = 0.05f;
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
  const float minDist = 15.0f;
  const float maxDist = 40.0f;
  const float repathInterval = 5.0f;

  Position *playerPos = ECS_GET(world, game->player, Position, COMP_POSITION);

  for (uint32_t i = 0; i < enemyArch->count; i++) {
    entity_t e = enemyArch->entities[i];

    Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
    if (!active || !active->value)
      continue;

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    NavPath *path = ECS_GET(world, e, NavPath, COMP_NAVPATH);
    Timer *repathTimer = ECS_GET(world, e, Timer, COMP_MOVE_TIMER);

    if (!pos || !path || !repathTimer)
      continue;

    // Only evaluate every N seconds
    if (repathTimer->value > 0.0f)
      continue;

    // Reset timer
    repathTimer->value = repathInterval;

    // ---- Distance check ----
    Vector3 toPlayer = Vector3Subtract(playerPos->value, pos->value);
    toPlayer.y = 0.0f;

    float distToPlayer = Vector3Length(toPlayer);

    // If already within band → keep current path
    if (distToPlayer >= minDist && distToPlayer <= maxDist)
      continue;

    // Clear previous path
    path->count = 0;
    path->currentIndex = 0;

    // ---- Try generating new ring target ----
    for (int attempt = 0; attempt < 10; attempt++) {
      float angle = GetRandomValue(0, 360) * DEG2RAD;
      float radius = GetRandomValue((int)minDist, (int)maxDist);

      Vector3 candidate = {playerPos->value.x + cosf(angle) * radius, 0.0f,
                           playerPos->value.z + sinf(angle) * radius};

      // Snap to terrain
      candidate.y = HeightMap_GetHeightCatmullRom(&game->terrainHeightMap,
                                                  candidate.x, candidate.z);

      int cx, cy;
      if (!NavGrid_WorldToCell(&game->navGrid, candidate, &cx, &cy))
        continue;

      int idx = NavGrid_Index(&game->navGrid, cx, cy);

      if (game->navGrid.cells[idx].type == NAV_CELL_WALL)
        continue;

      if (NavGrid_FindPath(&game->navGrid, pos->value, candidate, path)) {
        break; // success
      }
    }
  }
}
