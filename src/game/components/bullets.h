#include "../../engine/ecs/entity.h"

// index of these must match the enum below
static float bulletDamages[]    = {15.0f, 70.0f, 30.0f, 20.0f};
static float muzzleVelocities[] = {100.0f, 20.0f, 150.0f, 60.0f};

enum {
  BULLET_TYPE_STANDARD   = 0,
  BULLET_TYPE_MISSILE    = 1,
  BULLET_TYPE_AUTOCANNON = 2,
  BULLET_TYPE_PLASMA     = 3,
};

typedef struct {
  int   type;
  float shieldMult; // damage multiplier applied to shield HP
  float healthMult; // damage multiplier applied to health HP
  bool  pierce;     // future: bullet passes through targets without despawning
} BulletType;

typedef struct {
  entity_t owner;
  entity_t target;
  float turnSpeed; // radians/sec
  float maxSpeed;
} HomingMissile;
