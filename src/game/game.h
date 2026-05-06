
#pragma once
#include "../engine/ecs/archetype.h"
#include "../engine/ecs/component.h"
#include "../engine/ecs/component_registry.h"
#include "../engine/ecs/world.h"
#include "../engine/math/heightmap.h"
#include "../engine/sound/sound.h"
#include "systems/message_system.h"
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

typedef enum {
  MISSION_WAVES = 0,
  MISSION_EXPLORATION = 1,
} MissionType;

typedef struct {
  MissionType missionType;
  int currentWave; // 1-indexed; 0 = not started
  int enemiesAlive;
  float nextWaveTimer;
  bool waveActive;
  bool allWavesComplete;
} WaveState;

enum gameState {
  GAMESTATE_MAINMENU = 0,
  GAMESTATE_INLEVEL,
  GAMESTATE_SETTINGS,
  GAMESTATE_LOADING,
  GAMESTATE_EDITOR,
  GAMESTATE_LEVELSELECT,
  GAMESTATE_PAUSED,
};

typedef struct {
  world_t *world;
  Camera3D camera;

  componentPool_t timerPool;
  componentPool_t modelPool;

  ComponentRegistry componentRegistry;
} Engine;

typedef struct GameWorld {
  entity_t player;
  uint32_t playerActiveWeapon;
  enum gameState gameState;

  uint32_t targetLevel;
  char targetLevelPath[256];
  float loadingTimer;

  float masterVolume;
  int targetFPS;
  bool showFPS;

  float fov;
  int resWidth;
  int resHeight;
  bool fullscreen;
  enum gameState settingsPrevState;

  uint32_t playerArchId, bulletArchId, enemyCapsuleArchId, enemyGruntArchId,
      enemyMissileArchId, enemyRangerArchId, obstacleArchId, levelModelArchId,
      tutorialBoxArchId, missileArchId, wallSegArchId, spawnerArchId,
      particleArchId, enemyMeleeArchId, infoBoxArchId;

  WaveState waveState;

  float arenaRadius;

  int nextBulletIndex;

  Model bulletModel;
  Model enemyModel;
  Model gunModel;
  Model plasmaGunModel;
  Model shadowModel;

  Model missileEnemyModel;
  Model gruntTorso;
  Model gruntLegs;
  Model gruntGun;
  Model gruntSaw;

  HeightMap terrainHeightMap;
  Model terrainModel;
  char terrainModelPath[256];
  Model obstaclesModel;
  Model obstacleModel;
  Model infoBoxMarkerModel;

  NavGrid navGrid;

  SoundSystem_t soundSystem;
  MessageSystem_t messageSystem;
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
