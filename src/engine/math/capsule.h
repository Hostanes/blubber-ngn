
#pragma once
#include "math.h"
#include "raylib.h"
#include "raymath.h"

typedef struct {
  // -------- Local Space (relative to entity position) --------
  Vector3 localA; // bottom offset from entity origin
  Vector3 localB; // top offset from entity origin

  // -------- World Space (computed every frame) --------
  Vector3 worldA; // bottom endpoint in world space
  Vector3 worldB; // top endpoint in world space

  float radius;
} CapsuleCollider;

static inline void Capsule_UpdateWorld(CapsuleCollider *c, Vector3 position) {
  c->worldA = Vector3Add(position, c->localA);
  c->worldB = Vector3Add(position, c->localB);
}

static inline BoundingBox Capsule_ComputeAABB(const CapsuleCollider *c) {
  Vector3 min = {
      fminf(c->worldA.x, c->worldB.x) - c->radius,
      fminf(c->worldA.y, c->worldB.y) - c->radius,
      fminf(c->worldA.z, c->worldB.z) - c->radius,
  };

  Vector3 max = {
      fmaxf(c->worldA.x, c->worldB.x) + c->radius,
      fmaxf(c->worldA.y, c->worldB.y) + c->radius,
      fmaxf(c->worldA.z, c->worldB.z) + c->radius,
  };

  return (BoundingBox){min, max};
}
