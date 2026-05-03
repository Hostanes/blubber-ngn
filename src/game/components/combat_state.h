typedef enum {
  ENEMY_STATE_IDLE,
  ENEMY_STATE_MOVING,
  ENEMY_STATE_COMBAT, // Standing still and aiming
} EnemyState_e;

typedef struct {
  EnemyState_e state;
  float combatYaw;
  float moveYaw;
  float aimPitch;
  bool  isAiming;

  int   burstShotsRemaining;
  float burstTimer;
  int   burstType; // 0 = auto, 1 = missile

  float settleTimer;  // counts down after arriving; fire blocked while > 0
  bool  pathPending;  // true while a NavPathQueue request is in-flight
} CombatState_t;
