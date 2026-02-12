#pragma once
#include "raylib.h"
#include "raymath.h"

typedef struct {
  Vector3 center;
  float radius;
} SphereCollider;

static inline BoundingBox Sphere_ComputeAABB(const SphereCollider *s) {
  Vector3 r = {s->radius, s->radius, s->radius};
  return (BoundingBox){Vector3Subtract(s->center, r), Vector3Add(s->center, r)};
}
