#pragma once
#include "../ecs/entity.h"
#include "raylib.h"
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
} CollisionInstance_t;
