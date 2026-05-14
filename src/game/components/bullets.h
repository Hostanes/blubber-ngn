#include "../../engine/ecs/entity.h"

// index of these must match the enum below
static float bulletDamages[]    = {15.0f, 70.0f, 30.0f, 20.0f, 12.0f, 10.0f};
static float muzzleVelocities[] = {300.0f, 20.0f, 150.0f, 150.0f, 100.0f, 110.0f};

enum {
  BULLET_TYPE_STANDARD   = 0, // player machine gun
  BULLET_TYPE_MISSILE    = 1, // homing missile
  BULLET_TYPE_AUTOCANNON = 2,
  BULLET_TYPE_PLASMA     = 3, // player plasma gun
  BULLET_TYPE_ENEMY      = 4, // all enemy projectiles
  BULLET_TYPE_BUCKSHOT   = 5, // blunderbuss pellets — slow, wide spread
};

typedef struct {
  int type;
  float shieldMult; // damage multiplier applied to shield HP
  float healthMult; // damage multiplier applied to health HP
  bool pierce;      // future: bullet passes through targets without despawning
} BulletType;

typedef struct {
  entity_t owner;
  entity_t target;
  float turnSpeed;
  float maxSpeed;
  float blastDamage; // max damage at blast center
  bool armed;   // true once within arming distance -- flies straight from here
  bool guided;  // false = fly straight (no homing), still explodes on impact
} HomingMissile;
