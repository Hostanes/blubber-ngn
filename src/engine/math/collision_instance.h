#pragma once
#include "../ecs/entity.h"
#include "aabb.h"
#include "capsule.h"
#include "collision_hit.h"
#include "raylib.h"
#include "sphere.h"
#include <stdint.h>

typedef enum {
  COLLIDER_AABB,
  COLLIDER_SPHERE,
  COLLIDER_CAPSULE,
} ColliderType;

typedef struct {
  entity_t owner; // owning entity (globally unique)
  ColliderType type;

  BoundingBox worldBounds; // ALWAYS axis-aligned (broadphase)
  void *shape;             // points to concrete collider data

  uint32_t layerMask;   // collision layers (optional but useful)
  uint32_t collideMask; // what this can collide with
} CollisionInstance;

static inline bool AABB_Overlap(BoundingBox a, BoundingBox b) {
  return (a.min.x <= b.max.x && a.max.x >= b.min.x) &&
         (a.min.y <= b.max.y && a.max.y >= b.min.y) &&
         (a.min.z <= b.max.z && a.max.z >= b.min.z);
}

void Collision_UpdateInstance(CollisionInstance *ci, Vector3 position,
                              Quaternion rotation);

bool CollisionTest(const CollisionInstance *a, const CollisionInstance *b,
                   CollisionHit *outHit);
