#pragma once
#include "raylib.h"
#include "raymath.h"

typedef struct {
  Vector3 halfExtents; // half-size in local space
} AABBCollider;

static inline BoundingBox AABB_ComputeWorld(const AABBCollider *aabb,
                                            Vector3 position) {
  Vector3 min = {
      position.x - aabb->halfExtents.x,
      position.y - aabb->halfExtents.y,
      position.z - aabb->halfExtents.z,
  };

  Vector3 max = {
      position.x + aabb->halfExtents.x,
      position.y + aabb->halfExtents.y,
      position.z + aabb->halfExtents.z,
  };

  return (BoundingBox){min, max};
}
