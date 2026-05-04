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
