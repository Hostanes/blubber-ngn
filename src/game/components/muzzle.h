#pragma once

#include "bullets.h"
#include "transform.h"

typedef struct {
  Position    positionOffset;
  Orientation weaponOffset;
  Orientation aimRot;
  Orientation worldRot;

  int   bulletType;
  float shieldMult;   // forwarded to BulletType on spawn
  float healthMult;
  bool  pierce;

  int   spreadCount;  // pellets per shot (1 = single)
  float spreadAngle;  // half-cone in radians

  float fireRate;     // shots/sec; 0 = click-to-fire (not held)
  float fireTimer;    // countdown between shots (runtime)

  // runtime computed
  Vector3 worldPosition;
  Vector3 forward;

} Muzzle_t;

typedef struct {
  int count;
  Muzzle_t *Muzzles;
} MuzzleCollection_t;
