
#pragma once
#include "Active.h"
#include "collision.h"
#include "health.h"
#include "movement.h"
#include "muzzle.h"
#include "renderable.h"
#include "timer.h"
#include "transform.h"

enum {
  COMP_POSITION = 0,
  COMP_VELOCITY,
  COMP_ORIENTATION,
  COMP_MODEL,
  COMP_COYOTETIMER,
  COMP_TIMER,
  COMP_AABB,
  COMP_BULLETTYPE,
  COMP_LIFETIME,
  COMP_ACTIVE,
  COMP_COLLISION_INSTANCE,
  COMP_CAPSULE_COLLIDER,
  COMP_AABB_COLLIDER,
  COMP_SPHERE_COLLIDER,
  COMP_ISGROUNDED,
  COMP_GRAVITY, // for gravity and colliding with terrain and obstacles from
  COMP_MUZZLES,
};
