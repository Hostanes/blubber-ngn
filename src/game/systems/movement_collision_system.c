#include "../game.h"
#include "systems.h"

void PlayerObstacleCollisionSystem(world_t *world, GameWorld *game) {
  entity_t player = game->player;

  Position *playerPos = ECS_GET(world, player, Position, COMP_POSITION);
  Velocity *vel = ECS_GET(world, player, Velocity, COMP_VELOCITY);
  bool *isgrounded = ECS_GET(world, player, bool, COMP_ISGROUNDED);

  CollisionInstance *playerCI =
      ECS_GET(world, player, CollisionInstance, COMP_COLLISION_INSTANCE);

  if (!playerCI)
    return;

  CapsuleCollider *cap = (CapsuleCollider *)playerCI->shape;

  // ------------------------------------------------------------
  // 1️⃣ Update capsule from current position
  // ------------------------------------------------------------

  cap->a = Vector3Add(playerPos->value, (Vector3){0, PLAYER_RADIUS, 0});
  cap->b = Vector3Add(playerPos->value,
                      (Vector3){0, PLAYER_HEIGHT - PLAYER_RADIUS, 0});

  playerCI->worldBounds = Capsule_ComputeAABB(cap);

  // ------------------------------------------------------------
  // 2️⃣ Resolve penetration (horizontal + general collision)
  // ------------------------------------------------------------

  archetype_t *obstacleArch = WorldGetArchetype(world, game->obstacleArchId);

  for (int i = 0; i < obstacleArch->count; ++i) {
    entity_t obstacle = obstacleArch->entities[i];

    CollisionInstance *obsCI =
        ECS_GET(world, obstacle, CollisionInstance, COMP_COLLISION_INSTANCE);

    if (!obsCI)
      continue;

    if (!AABB_Overlap(playerCI->worldBounds, obsCI->worldBounds))
      continue;

    CollisionHit hit;
    if (!CollisionTest(playerCI, obsCI, &hit))
      continue;

    // Push player out
    Vector3 correction = Vector3Scale(hit.normal, hit.penetration);
    playerPos->value = Vector3Add(playerPos->value, correction);

    // Rebuild capsule after correction
    cap->a = Vector3Add(playerPos->value, (Vector3){0, PLAYER_RADIUS, 0});
    cap->b = Vector3Add(playerPos->value,
                        (Vector3){0, PLAYER_HEIGHT - PLAYER_RADIUS, 0});
    playerCI->worldBounds = Capsule_ComputeAABB(cap);

    // If we hit something below us, cancel downward velocity
    if (hit.normal.y > 0.5f && vel->value.y < 0.0f) {
      vel->value.y = 0.0f;
    }
  }

  // ------------------------------------------------------------
  // 3️⃣ Unified Ground Check (Terrain + Obstacles)
  // ------------------------------------------------------------

  *isgrounded = false;

  float bestGroundY = -FLT_MAX;

  // --- Terrain ground ---
  float terrainY = HeightMap_GetHeightSmooth(
      &game->terrainHeightMap, playerPos->value.x, playerPos->value.z);

  bestGroundY = terrainY;

  // --- Obstacle ray ground ---
  Vector3 rayOrigin = Vector3Add(playerPos->value, (Vector3){0, 0.05f, 0});
  Ray ray = {rayOrigin, (Vector3){0, -1, 0}};
  float rayLength = 0.6f;

  for (int i = 0; i < obstacleArch->count; ++i) {
    entity_t obstacle = obstacleArch->entities[i];

    CollisionInstance *obsCI =
        ECS_GET(world, obstacle, CollisionInstance, COMP_COLLISION_INSTANCE);

    if (!obsCI)
      continue;

    RayCollision rc = GetRayCollisionBox(ray, obsCI->worldBounds);

    if (rc.hit && rc.distance <= rayLength && rc.normal.y > 0.5f) {
      if (rc.point.y > bestGroundY) {
        bestGroundY = rc.point.y;
      }
    }
  }

  // ------------------------------------------------------------
  // 4️⃣ Snap to ground (ONLY if falling or resting)
  // ------------------------------------------------------------

  if (vel->value.y <= 0.0f) {
    float footY = playerPos->value.y;

    if (footY <= bestGroundY + 0.05f) {
      playerPos->value.y = bestGroundY;
      vel->value.y = 0.0f;
      *isgrounded = true;

      // Rebuild capsule after snap
      cap->a = Vector3Add(playerPos->value, (Vector3){0, PLAYER_RADIUS, 0});
      cap->b = Vector3Add(playerPos->value,
                          (Vector3){0, PLAYER_HEIGHT - PLAYER_RADIUS, 0});
      playerCI->worldBounds = Capsule_ComputeAABB(cap);
    }
  }
}
