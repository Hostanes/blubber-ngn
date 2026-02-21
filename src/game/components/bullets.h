
// index of these is the bullet Type
static float bulletDamages[] = {50.0f, 50.0f};
static float muzzleVelocities[] = {50.0f, 100.0f};
static float dropRates[] = {0.0f, 50.0f};

enum {
  BULLET_TYPE_STANDARD = 0,
  BULLET_TYPE_MISSILE,
};

typedef struct {
  int type;
} BulletType;
