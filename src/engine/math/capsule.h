#pragma once
#include "math.h"
#include "raylib.h"

typedef struct {
  Vector3 a; // bottom endpoint (world space)
  Vector3 b; // top endpoint (world space)
  float radius;
} CapsuleCollider;

static inline BoundingBox Capsule_ComputeAABB(const CapsuleCollider *c) {
  Vector3 min = {
      fminf(c->a.x, c->b.x) - c->radius,
      fminf(c->a.y, c->b.y) - c->radius,
      fminf(c->a.z, c->b.z) - c->radius,
  };

  Vector3 max = {
      fmaxf(c->a.x, c->b.x) + c->radius,
      fmaxf(c->a.y, c->b.y) + c->radius,
      fmaxf(c->a.z, c->b.z) + c->radius,
  };

  return (BoundingBox){min, max};
}
