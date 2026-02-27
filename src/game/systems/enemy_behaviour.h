#include "../game.h"
#include "systems.h"

static const float moveSpeeds[] = {20.0f};
static const float rotateSpeeds[] = {10.0f};

enum {
  AI_GRUNT_INACTIVE = 0,
  AI_GRUNT_AGGRESSIVE,
  AI_GRUNT_COVER,
  AI_GRUNT_SEARCHING,
};

enum {
  AI_TANK_INACTIVE = 0,
  AI_TANK_AGGRESSIVE,
  AI_TANK_COVER,
  AI_TANK_SEARCHING,
};
