#include "../../engine/ecs/entity.h"

// index of these is the bullet Type
static float bulletDamages[] = {10.0f, 70.0f, 30.0f};
static float muzzleVelocities[] = {100.0f, 20.0f, 150.0f};

enum {
  BULLET_TYPE_STANDARD = 0,
  BULLET_TYPE_MISSILE,
  BULLET_TYPE_AUTOCANNON,
};

typedef struct {
  int type;
} BulletType;

typedef struct {
  entity_t owner;
  entity_t target;
  float turnSpeed; // radians/sec
  float maxSpeed;
} HomingMissile;
