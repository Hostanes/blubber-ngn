
#pragma once
#include "../../engine/ecs/world.h"
#include "../../engine/math/heightmap.h"
#include "Active.h"
#include "ai_targets.h"
#include "bullet_owner.h"
#include "collision.h"
#include "combat_state.h"
#include "event_triggers.h"
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
  COMP_HEALTH,
  COMP_COLLISION_INSTANCE,
  COMP_CAPSULE_COLLIDER,
  COMP_AABB_COLLIDER,
  COMP_SPHERE_COLLIDER,
  COMP_ISGROUNDED,
  COMP_GRAVITY, // for gravity and colliding with terrain and obstacles from
  COMP_MUZZLES,
  COMP_DASHTIMER,
  COMP_DASHCOOLDOWN,
  COMP_ISDASHING,
  COMP_NAVPATH,
  COMP_BULLET_OWNER,
  COMP_GRUNT_FIRE_TIMER,
  COMP_MOVE_TARGET,
  COMP_AIM_TARGET,
  COMP_MOVE_TIMER,
  COMP_ONDEATH,
  COMP_ONCOLLISION,
  COMP_COMBAT_STATE,
  COMP_TYPE_RANGER,
  COMP_TYPE_GRUNT,
  COMP_TYPE_PLAYER,
  COMP_HOMINGMISSILE,
  COMP_WALL_SEGMENT_COLLIDER,
};
