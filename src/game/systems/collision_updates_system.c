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
  Position *p = ECS_GET(world, e, Position, COMP_POSITION);
  CapsuleCollider *cap =
      ECS_GET(world, e, CapsuleCollider, COMP_CAPSULE_COLLIDER);
  CollisionInstance *ci =
      ECS_GET(world, e, CollisionInstance, COMP_COLLISION_INSTANCE);

  cap->a = p->value;
  cap->b = Vector3Add(p->value, (Vector3){0, 1.8f, 0});

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
