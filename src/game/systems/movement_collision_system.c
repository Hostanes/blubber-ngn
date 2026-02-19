#include "../game.h"
#include "systems.h"

static void *GetColliderShape(world_t *world, entity_t e,
                              CollisionInstance *ci) {
  switch (ci->type) {
  case COLLIDER_SPHERE:
    return ECS_GET(world, e, SphereCollider, COMP_SPHERE_COLLIDER);

  case COLLIDER_AABB:
    return ECS_GET(world, e, AABBCollider, COMP_AABB_COLLIDER);

  case COLLIDER_CAPSULE:
    return ECS_GET(world, e, CapsuleCollider, COMP_CAPSULE_COLLIDER);

  default:
    return NULL;
  }
}

static void BuildPlayerCapsule(Position *pos, CapsuleCollider *cap,
                               CollisionInstance *ci) {
  float eyeHeight = 1.65f;
  float totalHeight = 1.8f;

  float bottom = eyeHeight;
  float top = totalHeight - eyeHeight;

  cap->a = Vector3Add(pos->value, (Vector3){0, -bottom + cap->radius, 0});

  cap->b = Vector3Add(pos->value, (Vector3){0, top - cap->radius, 0});

  ci->worldBounds = Capsule_ComputeAABB(cap);
}

static void ResolveCapsuleVsObstacles(world_t *world, GameWorld *game,
                                      Position *pos, Velocity *vel,
                                      CapsuleCollider *cap,
                                      CollisionInstance *playerCI,
                                      bool verticalPhase) {
  bool *isgrounded = ECS_GET(world, game->player, bool, COMP_ISGROUNDED);
  archetype_t *arch = WorldGetArchetype(world, game->obstacleArchId);

  for (int i = 0; i < arch->count; ++i) {
    entity_t obstacle = arch->entities[i];

    CollisionInstance *obsCI =
        ECS_GET(world, obstacle, CollisionInstance, COMP_COLLISION_INSTANCE);

    if (!obsCI)
      continue;

    // Broadphase
    if (!AABB_Overlap(playerCI->worldBounds, obsCI->worldBounds))
      continue;

    CollisionHit hit;

    void *shapeA = cap;
    void *shapeB = ECS_GET(world, obstacle, AABBCollider, COMP_AABB_COLLIDER);

    if (!shapeB)
      continue;

    if (!CollisionTest(playerCI, shapeA, obsCI, shapeB, &hit))
      continue;

    // -------- Resolution --------

    if (verticalPhase) {
      // LANDING
      if (vel->value.y < 0.0f && hit.normal.y > 0.5f) {
        pos->value =
            Vector3Add(pos->value, Vector3Scale(hit.normal, hit.penetration));

        vel->value.y = 0.0f;
        *isgrounded = true;
      }
      // CEILING
      else if (vel->value.y > 0.0f && hit.normal.y < -0.5f) {
        pos->value =
            Vector3Add(pos->value, Vector3Scale(hit.normal, hit.penetration));

        vel->value.y = 0.0f;
      }
    } else {
      // Horizontal resolve
      if (fabsf(hit.normal.y) < 0.5f) {
        pos->value =
            Vector3Add(pos->value, Vector3Scale(hit.normal, hit.penetration));

        vel->value.x = 0.0f;
        vel->value.z = 0.0f;
      }
    }

    BuildPlayerCapsule(pos, cap, playerCI);
  }
}

void PlayerMoveAndCollide(world_t *world, GameWorld *game, float dt) {
  entity_t player = game->player;

  Position *pos = ECS_GET(world, player, Position, COMP_POSITION);
  Velocity *vel = ECS_GET(world, player, Velocity, COMP_VELOCITY);
  bool *isgrounded = ECS_GET(world, player, bool, COMP_ISGROUNDED);
  CapsuleCollider *cap =
      ECS_GET(world, player, CapsuleCollider, COMP_CAPSULE_COLLIDER);
  CollisionInstance *ci =
      ECS_GET(world, player, CollisionInstance, COMP_COLLISION_INSTANCE);

  *isgrounded = false;

  Vector3 delta = Vector3Scale(vel->value, dt);

  // ------------------------------------
  // HORIZONTAL MOVE
  // ------------------------------------
  pos->value.x += delta.x;
  pos->value.z += delta.z;

  BuildPlayerCapsule(pos, cap, ci);
  ResolveCapsuleVsObstacles(world, game, pos, vel, cap, ci, false);

  // ------------------------------------
  // VERTICAL MOVE
  // ------------------------------------
  pos->value.y += delta.y;

  BuildPlayerCapsule(pos, cap, ci);
  ResolveCapsuleVsObstacles(world, game, pos, vel, cap, ci, true);

  // ------------------------------------
  // TERRAIN COLLISION
  // ------------------------------------
  float terrainY = HeightMap_GetHeightSmooth(&game->terrainHeightMap,
                                             pos->value.x, pos->value.z);

  float eyeHeight = 1.65f;
  float footY = pos->value.y - eyeHeight;

  if (footY < terrainY) {
    pos->value.y = terrainY + eyeHeight;
    vel->value.y = 0.0f;
    *isgrounded = true;
  }

  BuildPlayerCapsule(pos, cap, ci);
}

