
#pragma once
#include "../engine/ecs/archetype.h"
#include "../engine/ecs/component.h"
#include "../engine/ecs/world.h"
#include "../engine/math/heightmap.h"
#include "../engine/util/bitset.h"
#include "components/components.h"
#include "ecs_get.h"
#include "game.h"
#include "nav_grid/nav.h"
#include "raylib.h"
#include "raymath.h"
#include "systems/systems.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PLAYER_RADIUS 0.35f
#define PLAYER_HEIGHT 2.0f

enum CollisionLayer {
  LAYER_PLAYER = 0,
  LAYER_WORLD = 1,
  LAYER_ENEMY = 2,
  LAYER_BULLET = 3,
  LAYER_TRIGGER = 4
};

enum gameState {
  GAMESTATE_MAINMENU = 0,
  GAMESTATE_INLEVEL,
  GAMESTATE_SETTINGS,
  GAMESTATE_LOADING,
};

typedef struct {
  world_t *world;
  Camera3D camera;

  componentPool_t timerPool;
  componentPool_t modelPool;
} Engine;

typedef struct GameWorld {
  entity_t player;
  uint32_t playerActiveWeapon;
  enum gameState gameState;

  uint32_t targetLevel;
  float loadingTimer;

  float masterVolume;
  int targetFPS;
  bool showFPS;

  uint32_t playerArchId, bulletArchId, enemyCapsuleArchId, enemyGruntArchId,
      enemyMissileArchId, enemyRangerArchId, obstacleArchId, levelModelArchId,
      tutorialBoxArchId;

  float arenaRadius;

  int nextBulletIndex;

  Model bulletModel;
  Model enemyModel;
  Model gunModel;

  Model missileEnemyModel;
  Model gruntTorso;
  Model gruntLegs;
  Model gruntGun;

  HeightMap terrainHeightMap;
  Model terrainModel;
  Model ArenaModel175;

  NavGrid navGrid;
} GameWorld;

static void TryKillEntity(world_t *world, entity_t e) {
  Active *active = ECS_GET(world, e, Active, COMP_ACTIVE);
  if (!active || !active->value)
    return;

  active->value = false;

  OnDeath *od = ECS_GET(world, e, OnDeath, COMP_ONDEATH);
  if (od && od->fn)
    od->fn(world, e);
}

static float LerpAngle(float from, float to, float t) {
  float difference = fmodf(to - from, 2 * PI);
  if (difference > PI)
    difference -= 2 * PI;
  if (difference < -PI)
    difference += 2 * PI;
  return from + difference * t;
}

void RunGameLoop(Engine *engine, GameWorld *game);

Engine EngineInit(void);
void EngineShutdown(Engine *engine);
