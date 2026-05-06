#pragma once
#include "../../engine/ecs/world.h"

typedef struct {
  entity_t target;
  bool     hasTarget;
  float    retargetTimer;
  float    bobTimer;
} DroneEnemy;
