
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

#define NUM_ARCHETYPES 1024
#define TOTAL_ENTITIES 1000000

enum CollisionLayer {
  LAYER_PLAYER = 0,
  LAYER_WORLD = 1,
  LAYER_ENEMY = 2,
  LAYER_BULLET = 3
};

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

  uint32_t archIds[NUM_ARCHETYPES];
  uint32_t archCount;

  Model bulletModel;
  Model enemyModel;
  Model gunModel;

  Model missileEnemyModel;
  Model gruntTorso;
  Model gruntLegs;
  Model gruntGun;

  HeightMap terrainHeightMap;
  Model terrainModel;

  NavGrid navGrid;
} GameWorld;

void RunGameLoop(Engine *engine, GameWorld *game);

Engine EngineInit(void);
void EngineShutdown(Engine *engine);

GameWorld GameWorldCreate(Engine *engine, world_t *world);
