
#pragma once
#include "raylib.h"

typedef struct {
  BoundingBox local; // local-space AABB
  BoundingBox world; // cached world-space AABB
  bool dirty;
} AABBCollider;
