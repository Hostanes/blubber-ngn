#include "../game.h"
#include "raylib.h"
#include "systems.h"

static bitset_t activeMask;
static bool activeMaskInit = false;

static void EnsureActiveMask(void) {
  if (activeMaskInit)
    return;

  BitsetInit(&activeMask, 64);
  BitsetSet(&activeMask, COMP_ACTIVE);

  activeMaskInit = true;
}

void UpdatePlayerCollision(world_t *world, entity_t e) {
  Position *playerPos = ECS_GET(world, e, Position, COMP_POSITION);
  CapsuleCollider *cap =
      ECS_GET(world, e, CapsuleCollider, COMP_CAPSULE_COLLIDER);
  CollisionInstance *ci =
      ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);

  float eyeHeight = 1.65f;  // distance from feet to eyes
  float totalHeight = 1.8f; // total capsule height

  float bottom = eyeHeight;            // how far down to feet
  float top = totalHeight - eyeHeight; // how far up to top

  cap->worldA =
      Vector3Add(playerPos->value, (Vector3){0, -bottom + PLAYER_RADIUS, 0});

  cap->worldB =
      Vector3Add(playerPos->value, (Vector3){0, top - PLAYER_RADIUS, 0});

  ci->worldBounds = Capsule_ComputeAABB(cap);
}

void UpdateObstacleCollision(world_t *world, archetype_t *obstacleArch) {

  for (int i = 0; i < obstacleArch->count; i++) {

    entity_t e = obstacleArch->entities[i];

    bool hasActive = BitsetContainsAll(&obstacleArch->mask, &activeMask);

    if (hasActive) {
      Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
      if (!active->value)
        continue;
    }

    Position *p = ECS_GET(world, e, Position, COMP_POSITION);
    AABBCollider *aabb = ECS_GET(world, e, AABBCollider, COMP_AABB_COLLIDER);
    CollisionInstance *ci =
        ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);

    ci->worldBounds = (BoundingBox){.min =
                                        {
                                            p->value.x - aabb->halfExtents.x,
                                            p->value.y - aabb->halfExtents.y,
                                            p->value.z - aabb->halfExtents.z,
                                        },
                                    .max = {
                                        p->value.x + aabb->halfExtents.x,
                                        p->value.y + aabb->halfExtents.y,
                                        p->value.z + aabb->halfExtents.z,
                                    }};
  }
}

// void UpdateBulletCollision(world_t *world, archetype_t *bulletArch) {
//   for (int i = 0; i < bulletArch->count; i++) {
//     entity_t e = bulletArch->entities[i];

//     Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
//     if (!active || !active->value)
//       continue;

//     Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
//     SphereCollider *sphere =
//         ECS_GET(world, e, SphereCollider, COMP_SPHERE_COLLIDER);
//     CollisionInstance *ci =
//         ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);

//     // Update sphere center from position
//     sphere->center = pos->value;

//     // Update broadphase bounds
//     ci->worldBounds = Sphere_ComputeAABB(sphere);
//     // printf("Bullet AABB min: %.2f %.2f %.2f\n", ci->worldBounds.min.x,
//     //        ci->worldBounds.min.y, ci->worldBounds.min.z);
//   }
// }

void UpdateCollisionBounds(world_t *world) {
  for (int a = 0; a < world->archetypeCount; a++) {
    archetype_t *arch = &world->archetypes[a];

    if (!ArchetypeHas(arch, COMP_COLLISION_INSTANCE))
      continue;

    for (int i = 0; i < arch->count; i++) {
      entity_t e = arch->entities[i];

      // Skip inactive
      if (ArchetypeHas(arch, COMP_ACTIVE)) {
        Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
        if (!active || !active->value)
          continue;
      }

      CollisionInstance *ci =
          ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);

      if (!ci)
        continue;

      Position *pos = ECS_GET(world, e, Position, COMP_POSITION);

      if (!pos)
        continue;

      switch (ci->type) {
      case COLLIDER_SPHERE: {
        SphereCollider *sphere =
            ECS_GET(world, e, SphereCollider, COMP_SPHERE_COLLIDER);

        if (!sphere)
          break;

        sphere->center = pos->value;
        ci->worldBounds = Sphere_ComputeAABB(sphere);
      } break;

      case COLLIDER_AABB: {
        AABBCollider *aabb =
            ECS_GET(world, e, AABBCollider, COMP_AABB_COLLIDER);

        if (!aabb)
          break;

        ci->worldBounds.min = (Vector3){pos->value.x - aabb->halfExtents.x,
                                        pos->value.y - aabb->halfExtents.y,
                                        pos->value.z - aabb->halfExtents.z};

        ci->worldBounds.max = (Vector3){pos->value.x + aabb->halfExtents.x,
                                        pos->value.y + aabb->halfExtents.y,
                                        pos->value.z + aabb->halfExtents.z};
      } break;

      case COLLIDER_CAPSULE: {
        CapsuleCollider *cap =
            ECS_GET(world, e, CapsuleCollider, COMP_CAPSULE_COLLIDER);

        if (!cap)
          break;

        // If capsule endpoints depend on position, rebuild them here
        // (only if not already done in movement system)

        ci->worldBounds = Capsule_ComputeAABB(cap);
      } break;

      default:
        break;
      }
    }
  }
}

void CollisionSyncSystem(world_t *world) {
  for (int a = 0; a < world->archetypeCount; a++) {
    archetype_t *arch = &world->archetypes[a];

    if (!ArchetypeHas(arch, COMP_COLLISION_INSTANCE))
      continue;
    if (!ArchetypeHas(arch, COMP_VELOCITY)) {
      continue;
    }

    for (uint32_t i = 0; i < arch->count; i++) {
      entity_t e = arch->entities[i];

      // Skip inactive
      if (ArchetypeHas(arch, COMP_ACTIVE)) {
        Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
        if (!active || !active->value)
          continue;
      }

      CollisionInstance *ci =
          ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);

      Position *pos = ECS_GET(world, e, Position, COMP_POSITION);

      if (!ci || !pos)
        continue;

      switch (ci->type) {
      case COLLIDER_AABB: {
        AABBCollider *aabb =
            ECS_GET(world, e, AABBCollider, COMP_AABB_COLLIDER);

        if (!aabb)
          break;

        Collision_UpdateAABB(ci, aabb, pos->value);
      } break;

      case COLLIDER_SPHERE: {
        SphereCollider *sphere =
            ECS_GET(world, e, SphereCollider, COMP_SPHERE_COLLIDER);

        if (!sphere)
          break;

        Collision_UpdateSphere(ci, sphere, pos->value);
      } break;

      case COLLIDER_CAPSULE: {
        CapsuleCollider *cap =
            ECS_GET(world, e, CapsuleCollider, COMP_CAPSULE_COLLIDER);

        if (!cap)
          break;

        Capsule_UpdateWorld(cap, pos->value);
        ci->worldBounds = Capsule_ComputeAABB(cap);
      } break;
      }
    }
  }
}
