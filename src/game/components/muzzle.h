#pragma once

#include "bullets.h"
#include "transform.h"

typedef struct {
  Position positionOffset;
  Orientation oriOffset;
  int bulletType;

  // runtime computed
  Vector3 worldPosition;
  Vector3 forward;
} Muzzle_t;

typedef struct {
  int count;
  Muzzle_t *Muzzles;
} MuzzleCollection_t;
