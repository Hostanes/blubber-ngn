#pragma once
#include <raylib.h>
#include <stdbool.h>

typedef enum {
  MELEE_CHASING    = 0,
  MELEE_WINDING_UP = 1,
  MELEE_LUNGING    = 2,
  MELEE_RECOVERING = 3,
} MeleeState_e;

typedef struct {
  MeleeState_e state;
  float        windupTimer;
  float        lungeTimer;
  float        recoverTimer;
  Vector3      lungeTarget;
  bool         hasHit;
  bool         pathPending;
  float        repathTimer;
} MeleeEnemy;
