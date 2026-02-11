
#pragma once
#include "raylib.h"

typedef struct {
  Vector3 localCenter;
  float radius;

  Vector3 worldCenter;
  bool dirty;
} SphereCollider;
