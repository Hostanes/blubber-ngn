#include <stdint.h>

typedef enum {
  ENEMY_STATE_IDLE    = 0,
  ENEMY_STATE_MOVING  = 1,
  ENEMY_STATE_COMBAT  = 2,
  // Tactical states for grunt/ranger (melee uses its own MeleeState enum)
  ENEMY_AI_ADVANCE    = 3,
  ENEMY_AI_SUPPRESS   = 4,
  ENEMY_AI_COVER      = 5,
  ENEMY_AI_RETREAT    = 6,
  ENEMY_AI_REPOSITION = 7,
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

  int16_t claimedCX, claimedCY;    // nav cell this enemy has claimed (-1 = none)
  float   repositionTimer;          // countdown to next position re-evaluation
  float   losCheckTimer;            // throttles LOS raycasts
  bool    hasLOS;                   // last LOS result (cached)
} CombatState_t;
