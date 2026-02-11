
#pragma once
#include "raylib.h"

typedef struct {
  float radius;
  float halfHeight; // center → sphere center

  Vector3 localCenter;
  Vector3 worldCenter;

  bool dirty;
} CapsuleCollider;
