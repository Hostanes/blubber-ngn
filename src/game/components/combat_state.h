typedef enum {
  ENEMY_STATE_IDLE,
  ENEMY_STATE_MOVING,
  ENEMY_STATE_COMBAT, // Standing still and aiming
} EnemyState_e;

typedef struct {
  EnemyState_e state;
  float combatYaw; // Target yaw for aiming
  float moveYaw;   // Target yaw for moving
  float aimPitch;  // Current muzzle pitch
  bool isAiming;   // Threshold check flag
} CombatState_t;
