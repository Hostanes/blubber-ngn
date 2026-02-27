#pragma once

#include "bullets.h"
#include "transform.h"

typedef struct {
  Position positionOffset; // mount position relative to entity

  Orientation weaponOffset; // static mount bias (constant)
  Orientation aimRot;       // dynamic aim (changes every frame)

  Orientation worldRot; // final computed world rotation

  int bulletType;

  // runtime computed
  Vector3 worldPosition;
  Vector3 forward;

} Muzzle_t;

typedef struct {
  int count;
  Muzzle_t *Muzzles;
} MuzzleCollection_t;
