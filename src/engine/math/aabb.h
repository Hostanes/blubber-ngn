
#pragma once
#include "raylib.h"
#include "raymath.h"

typedef struct {
  Vector3 min;
  Vector3 max;
} AABB;

static inline AABB AABB_FromCenterHalfExtents(Vector3 c, Vector3 e) {
  return (AABB){.min = Vector3Subtract(c, e), .max = Vector3Add(c, e)};
}

int AABB_Intersect(AABB a, AABB b);
