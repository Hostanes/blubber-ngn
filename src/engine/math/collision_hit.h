
#pragma once
#include "../ecs/entity.h"
#include "raylib.h"

typedef struct {
  entity_t a;
  entity_t b;

  Vector3 normal;    // from A → B
  float penetration; // overlap depth
  Vector3 point;     // approximate contact point
} CollisionHit;
