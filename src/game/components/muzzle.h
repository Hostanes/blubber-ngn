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

  float recoil;       // current recoil displacement (0 = rest, decays back each frame)

  float heat;               // 0.0 = cold, 1.0 = overheated (runtime)
  float heatPerShot;        // heat added per shot fired
  float coolRate;           // heat/s when weapon never reached 100%
  float coolRateOverheated; // heat/s after hitting 100% (slower)
  float overheatThreshold;  // heat must drop below this before firing resumes after overheat
  float coolDelay;          // seconds after last shot before cooling begins
  float coolDelayTimer;     // runtime: counts down to 0, then cooling starts
  bool  isOverheated;       // runtime: true from 100% until heat < overheatThreshold
  float smokeTimer;         // runtime: countdown between smoke particle spawns

  // runtime computed
  Vector3 worldPosition;
  Vector3 forward;

} Muzzle_t;

typedef struct {
  int count;
  Muzzle_t *Muzzles;
} MuzzleCollection_t;
