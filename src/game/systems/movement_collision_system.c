#include "../game.h"
#include "systems.h"

void PlayerObstacleCollisionSystem(world_t *world, GameWorld *game) {

  entity_t player = game->player; // or however you track it

  Position *playerPos = ECS_GET(world, player, Position, COMP_POSITION);

  CollisionInstance *playerCI =
      ECS_GET(world, player, CollisionInstance, COMP_COLLISION_INSTANCE);

  if (!playerCI)
    return;

  CapsuleCollider *cap = (CapsuleCollider *)playerCI->shape;

  cap->a = Vector3Add(playerPos->value, (Vector3){0, PLAYER_RADIUS, 0});
  cap->b = Vector3Add(playerPos->value,
                      (Vector3){0, PLAYER_HEIGHT - PLAYER_RADIUS, 0});

  archetype_t *obstacleArch = WorldGetArchetype(world, game->obstacleArchId);

  for (int i = 0; i < obstacleArch->count; ++i) {
    entity_t obstacle = obstacleArch->entities[i];

    CollisionInstance *obsCI =
        ECS_GET(world, obstacle, CollisionInstance, COMP_COLLISION_INSTANCE);

    if (!obsCI)
      continue;

    // 3. Broadphase: AABB vs AABB
    printf("Checking aabb overlap\n");
    if (!AABB_Overlap(playerCI->worldBounds, obsCI->worldBounds))
      continue;

    // 4. Narrowphase: Capsule vs AABB

    printf("Checking caspule x aabb overlap\n");
    CollisionHit hit;
    if (!CollisionTest(playerCI, obsCI, &hit))
      continue;

    // 5. Resolve penetration

    printf("Hit!!\n");

    // Push player out of obstacle
    Vector3 correction = Vector3Scale(hit.normal, hit.penetration);

    playerPos->value = Vector3Add(playerPos->value, correction);

    cap->a = playerPos->value;
    cap->b = Vector3Add(playerPos->value, (Vector3){0, 1.8f, 0});

    playerCI->worldBounds = Capsule_ComputeAABB(cap);
  }
}
