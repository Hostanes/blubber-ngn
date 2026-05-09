#include "../game.h"
#include "systems.h"

// [0]=grunt, [1]=ranger
static const float moveSpeeds[]      = {20.0f, 15.0f};
static const float rotateSpeeds[]    = {10.0f,  8.0f};
static const float bodyAimSpeeds[]   = { 5.0f,  4.0f};
static const float muzzleAimSpeeds[] = { 8.0f,  6.0f};

// Seconds after arriving at destination before firing is allowed
#define ENEMY_SETTLE_TIME        0.5f

// Movement smoothing
#define ENEMY_DECEL_DIST         4.0f    // world units — ramp-down zone before final waypoint
#define ENEMY_MIN_SPEED_FACTOR   0.15f   // minimum speed fraction during deceleration

// Firing alignment thresholds
#define ENEMY_BODY_YAW_THRESHOLD 0.85f   // cos(~32°) — body must face player to fire
#define ENEMY_AIM_THRESHOLD      0.50f   // muzzle forward dot product threshold

// Grunt tactical AI
#define GRUNT_MIN_DIST            5.0f
#define GRUNT_MAX_DIST            70.0f
#define GRUNT_PANIC_DIST          9.0f
#define GRUNT_LOW_HP_FRAC         0.25f
#define GRUNT_REPOSITION_BASE     7.0f
#define GRUNT_REPOSITION_JITTER   3.0f
#define GRUNT_LOS_REPOSITION      2.5f
#define GRUNT_LOS_CHECK_INTERVAL  0.40f
#define TACTICAL_SEARCH_RADIUS    45
#define GRUNT_MAX_MOVE_RADIUS     42.0f   // max world-units moved per reposition cycle
#define RANGER_MAX_MOVE_RADIUS    58.0f

// Ranger tactical AI
#define RANGER_MIN_DIST           20.0f
#define RANGER_MAX_DIST           130.0f
#define RANGER_PANIC_DIST         16.0f
#define RANGER_LOW_HP_FRAC        0.35f
#define RANGER_REPOSITION_BASE    10.0f
#define RANGER_REPOSITION_JITTER  4.0f
#define RANGER_LOS_REPOSITION     3.0f
#define RANGER_LOS_CHECK_INTERVAL 0.50f
