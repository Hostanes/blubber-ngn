
#pragma once
#include "../engine/ecs/archetype.h"
#include "../engine/ecs/component.h"
#include "../engine/ecs/world.h"
#include "../engine/math/heightmap.h"
#include "../engine/util/bitset.h"
#include "components/components.h"
#include "components/movement.h"
#include "components/renderable.h"
#include "components/transform.h"
#include "ecs_get.h"
#include "game.h"
#include "raylib.h"
#include "raymath.h"
#include "systems/systems.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define PLAYER_RADIUS 0.35f
#define PLAYER_HEIGHT 2.0f

enum gameState {
  GAMESTATE_MAINMENU = 0,
  GAMESTATE_INLEVEL,
};

typedef struct {
  world_t *world;
  Camera3D camera;

  componentPool_t timerPool;
  componentPool_t modelPool;
} Engine;

typedef struct GameWorld {
  entity_t player;
  enum gameState gameState;

  int nextBulletIndex;

  uint32_t playerArchId;
  uint32_t obstacleArchId;
  uint32_t bulletArchId;

  HeightMap terrainHeightMap;
  Model terrainModel;
} GameWorld;

void RunGameLoop(Engine *engine, GameWorld *game);

Engine EngineInit(void);
void EngineShutdown(Engine *engine);

GameWorld GameWorldCreate(Engine *engine, world_t *world);
