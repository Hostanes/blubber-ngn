// game.h
// ECS-style core game state and components

#pragma once
#include "engine.h"
#include "engine_components.h"
#include "raylib.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static const Vector3 DETECTION_CENTER = {0, 0, -2000}; // map point, not tank
static const float DETECTION_RADIUS = 4000.0f;

static const Vector3 IDLE_POINT = {0, 0, -2000}; // where tank goes in idle

static const float CIRCLE_RADIUS = 1500.0f;
static const float CHARGE_DURATION = 2.0f;  // seconds of charging
static const float CHARGE_COOLDOWN = 15.0f; // time between charges

static const Vector3 PARK_POS = {999999.0f, -10000.0f, 999999.0f};

// Forward declare GameState_t so callbacks can reference it
typedef struct GameState GameState_t;

//----------------------------------------
// Terrain
//----------------------------------------
typedef struct {
  Mesh mesh;
  Model model;
  float *height;
  float minX, minZ;
  int hmWidth, hmHeight;
  float cellSizeX, cellSizeZ;
  float worldWidth, worldLength;
} Terrain_t;

//----------------------------------------
// Banner
//----------------------------------------

typedef enum {
  BANNER_HIDDEN,
  BANNER_SLIDE_IN,
  BANNER_VISIBLE,
  BANNER_SLIDE_OUT
} BannerState;

typedef struct {
  char text[256];

  BannerState state;

  float timer;       // counts time while visible
  float visibleTime; // how long message stays up

  float y;       // current Y of banner
  float targetY; // y-position when fully shown
  float hiddenY; // y-position when fully hidden

  float speed; // slide speed
  bool active;
} MessageBanner_t;

//----------------------------------------
// CallBacks
//----------------------------------------

typedef void (*OnCollisionFn)(Engine_t *, GameState_t *gs, entity_t self,
                              entity_t other, char *text);
typedef void (*OnCollisionExitFn)(Engine_t *, GameState_t *gs, entity_t self,
                                  entity_t other);
typedef void (*OnDeathFn)(Engine_t *, GameState_t *gs, entity_t self);

typedef struct {
  OnCollisionFn onCollision;
  OnCollisionExitFn onCollisionExit;
  bool isColliding;

  OnDeathFn onDeath;
} BehaviorCallBacks_t;

//----------------------------------------
// Grid
//----------------------------------------
typedef struct {
  int entities[256];
  int count;
} GridNode_t;

typedef struct {
  GridNode_t **nodes;
  float cellSize;
  float minX, minZ;
  int width, length;
} EntityGrid_t;

// Component IDs are stored here
// Component values themselves are stored in engine -> actors -> Component Store
typedef struct {
  int cid_Positions;
  int cid_prevPositions;
  int cid_velocities;

  int cid_stepCycle;
  int cid_prevStepCycle;
  int cid_stepRate;

  int cid_weaponCount;
  int cid_weaponDamage;

  int cid_behavior;

  int cid_moveTarget;
  int cid_moveTimer;
  int cid_moveBehaviour;

  int cid_aiTimer;

  int cid_aimTarget;
  int cid_aimError;

  // TODO add others
} ActorComponentRegistry_t;

typedef enum {
  PSTATE_NORMAL = 0,
  PSTATE_DASH_CHARGE = 1,
  PSTATE_DASH_GO = 2,
  PSTATE_DASH_SLOW = 3
} PlayerMoveState;

typedef enum {
  TANK_IDLE = 0,
  TANK_ALERT_CIRCLE = 1,
  TANK_ALERT_CHARGE = 2
} TankAIState;

// Wave system defs

typedef enum {
  WAVE_WAITING = 0,  // waiting for next wave
  WAVE_SPAWNING = 1, // just triggered wave start function
  WAVE_ACTIVE = 2,   // enemies alive, fighting
  WAVE_COMPLETE = 3, // cleared, about to start break timer
  WAVE_FINISHED = 4  // all waves done
} WaveState;

#define MAX_WAVES 16
#define MAX_POOL_TANKS 32
#define MAX_POOL_HARASSERS 64
#define MAX_POOL_ALPHA 8

typedef struct {
  WaveState state;

  int waveIndex;
  int totalWaves;

  float betweenWaveTimer; // countdown
  float betweenWaveDelay; // per-wave delay (or global)

  int enemiesAliveThisWave; // decremented on death (best), or recomputed

  // pools
  entity_t tankPool[MAX_POOL_TANKS];
  bool tankUsed[MAX_POOL_TANKS];

  entity_t harasserPool[MAX_POOL_HARASSERS];
  bool harasserUsed[MAX_POOL_HARASSERS];

  entity_t alphaPool[MAX_POOL_ALPHA];
  bool alphaUsed[MAX_POOL_ALPHA];
} WaveSystem_t;

typedef struct {
  int index;    // current tip
  int count;    // number of tips
  bool visible; // optional toggle later
} UITips_t;

//----------------------------------------
// Game State
//----------------------------------------
typedef struct GameState {
  int playerId;
  AllState_t state;
  float pHeadbobTimer;

  ActorComponentRegistry_t compReg;

  Terrain_t terrain;

  EntityGrid_t grid;

  MessageBanner_t banner;

  bool isZooming;
  float heatMeter;

  Shader outlineShader;

  WaveSystem_t waves;

  Texture2D tankAimerTex;
  Texture2D hudDamagedTex;

  UITips_t tips;

  bool paused;

} GameState_t;

static const char *gTips[] = {
    "MOVEMENT\nW/S: move forward/back\nA/D: strafe\nSHIFT: sprint",
    "AIM\nMouse aims your mech.\nCrosshair shows aim point.",

    "DASH\nSPACE: dash in movement direction.\nUse it to reposition.",

    "WEAPONS\nLMB: left gun\nRMB: cannon\nQ: rocket\nE: blunderbuss\nGuns are "
    "slightly offcenter\nthey wont shoot exactly at the crosshair",

    "HEAT\nFiring and dashing builds HEAT.\nIf heat is high, you must cool"
    "down.",

    "ZOOM\nB: toggle binocular zoom\nZoom lowers sensitivity."};
static const int gTipsCount = (int)(sizeof(gTips) / sizeof(gTips[0]));

// damage each projectile does by type
static int projectileDamage[] = {0, 5, 20, 15, 30, 2};

typedef enum {
  P_BULLET = 1,
  P_PLASMA = 2,
  P_ROCKET = 3,
  P_MISSILE = 4
} ProjectileType;

//----------------------------------------
// Game Initialization
//----------------------------------------
GameState_t InitGameSimulator(Engine_t *eng);
GameState_t InitGameDuel(Engine_t *eng);
void StartGameTutorial(GameState_t *gs, Engine_t *eng);
Vector3 ConvertOrientationToVector3(Orientation o);
void StartGameDuel(GameState_t *gs, Engine_t *eng);
void ResetGameDuel(GameState_t *gs, Engine_t *eng);
static void FreeActorDynamicData(Engine_t *eng);

void Wave1Start(GameState_t *gs, Engine_t *eng);
void Wave2Start(GameState_t *gs, Engine_t *eng);
void Wave3Start(GameState_t *gs, Engine_t *eng);
void Wave4Start(GameState_t *gs, Engine_t *eng);
void Wave5Start(GameState_t *gs, Engine_t *eng);
void Wave6Start(GameState_t *gs, Engine_t *eng);

//----------------------------------------
// Grid Initialization
//----------------------------------------

#define GRID_CELL_SIZE 200.0f
#define MAX_GRID_NODES 128
#define GRID_EMPTY -1

static inline void AllocGrid(EntityGrid_t *grid, Terrain_t *terrain,
                             float cellSize) {
  grid->cellSize = cellSize;
  grid->minX = terrain->minX;
  grid->minZ = terrain->minZ;

  // Compute grid size from terrain dimensions
  grid->width = (int)ceilf(terrain->worldWidth / cellSize);
  grid->length = (int)ceilf(terrain->worldLength / cellSize);

  // Allocate nodes dynamically
  grid->nodes = (GridNode_t **)malloc(grid->width * sizeof(GridNode_t *));
  for (int x = 0; x < grid->width; x++) {
    grid->nodes[x] = (GridNode_t *)malloc(grid->length * sizeof(GridNode_t));
    for (int z = 0; z < grid->length; z++) {
      grid->nodes[x][z].count = 0;
      for (int i = 0; i < MAX_GRID_NODES; i++)
        grid->nodes[x][z].entities[i] = GRID_EMPTY;
    }
  }
}

static inline void DestroyGrid(EntityGrid_t *grid) {
  if (!grid->nodes)
    return;
  for (int x = 0; x < grid->width; x++)
    free(grid->nodes[x]);
  free(grid->nodes);
  grid->nodes = NULL;
  grid->width = grid->length = 0;
}

//----------------------------------------
// Grid Helpers
//----------------------------------------
static inline bool IsCellValid(EntityGrid_t *grid, int x, int z) {
  return x >= 0 && x < grid->width && z >= 0 && z < grid->length;
}

static inline bool GridAddEntity(EntityGrid_t *grid, entity_t e, Vector3 pos) {
  int ix = (int)((pos.x - grid->minX) / grid->cellSize);
  int iz = (int)((pos.z - grid->minZ) / grid->cellSize);

  if (!IsCellValid(grid, ix, iz))
    return false;

  GridNode_t *node = &grid->nodes[ix][iz];

  if (node->count >= MAX_GRID_NODES) {
    // DEBUG: show overflow + who is being dropped
    printf("[GRID] cell full (%d,%d) dropping entity=%d\n", ix, iz, (int)e);
    return false;
  }

  node->entities[node->count++] = e;
  return true;
}

static inline void GridRemoveEntity(EntityGrid_t *grid, entity_t e,
                                    Vector3 pos) {
  int ix = (int)((pos.x - grid->minX) / grid->cellSize);
  int iz = (int)((pos.z - grid->minZ) / grid->cellSize);

  if (!IsCellValid(grid, ix, iz))
    return;

  GridNode_t *node = &grid->nodes[ix][iz];
  for (int i = 0; i < node->count; i++) {
    if (node->entities[i] == e) {
      node->entities[i] = node->entities[--node->count];
      node->entities[node->count] = GRID_EMPTY;
      return;
    }
  }
}

static inline GridNode_t *FindGridNodeFromPosition(EntityGrid_t *grid,
                                                   Vector3 pos) {
  int ix = (int)((pos.x - grid->minX) / grid->cellSize);
  int iz = (int)((pos.z - grid->minZ) / grid->cellSize);

  if (!IsCellValid(grid, ix, iz))
    return NULL;
  return &grid->nodes[ix][iz];
}

static inline void UpdateEntityInGrid(GameState_t *gs, Engine_t *eng,
                                      entity_t e) {
  int idx = GetEntityIndex(e);
  EntityType_t type = eng->actors->types[idx];

  if (type != ENTITY_PLAYER && type != ENTITY_HARASSER && type != ENTITY_TANK &&
      type != ENTITY_TANK_ALPHA)
    return;

  Vector3 *prevPos =
      (Vector3 *)getComponent(eng->actors, e, gs->compReg.cid_prevPositions);

  Vector3 *currPos =
      (Vector3 *)getComponent(eng->actors, e, gs->compReg.cid_Positions);

  GridRemoveEntity(&gs->grid, e, *prevPos);

  GridAddEntity(&gs->grid, e, *currPos);

  *prevPos = *currPos;
}

static void ClearGrid(EntityGrid_t *grid) {
  if (!grid || !grid->nodes)
    return;
  for (int x = 0; x < grid->width; x++) {
    for (int z = 0; z < grid->length; z++) {
      grid->nodes[x][z].count = 0;
      for (int i = 0; i < MAX_GRID_NODES; i++) {
        grid->nodes[x][z].entities[i] = GRID_EMPTY;
      }
    }
  }
}

//
//

static void ReleaseTank(GameState_t *gs, entity_t e) {
  for (int i = 0; i < MAX_POOL_TANKS; i++) {
    if (gs->waves.tankPool[i] == e) {
      gs->waves.tankUsed[i] = false;
      return;
    }
  }
}

static void ReleaseHarasser(GameState_t *gs, entity_t e) {
  for (int i = 0; i < MAX_POOL_HARASSERS; i++) {
    if (gs->waves.harasserPool[i] == e) {
      gs->waves.harasserUsed[i] = false;
      return;
    }
  }
}

static void ReleaseAlphaTank(GameState_t *gs, entity_t e) {
  for (int i = 0; i < MAX_POOL_ALPHA; i++) {
    if (gs->waves.alphaPool[i] == e) {
      gs->waves.alphaUsed[i] = false;
      return;
    }
  }
}

static inline void DeactivateEntity(GameState_t *gs, Engine_t *eng,
                                    entity_t e) {
  int idx = GetEntityIndex(e);

  // remove from grid first (if it was in)
  Vector3 *pos =
      (Vector3 *)getComponent(eng->actors, e, gs->compReg.cid_Positions);
  if (pos)
    GridRemoveEntity(&gs->grid, e, *pos);

  // park it
  if (pos)
    *pos = PARK_POS;

  Vector3 *prev =
      (Vector3 *)getComponent(eng->actors, e, gs->compReg.cid_prevPositions);
  if (prev)
    *prev = PARK_POS;

  // set ECS alive flag off
  eng->em.alive[idx] = 0;
}

static inline void ActivateEntityAt(GameState_t *gs, Engine_t *eng, entity_t e,
                                    Vector3 worldPos) {
  int idx = GetEntityIndex(e);

  // mark alive
  eng->em.alive[idx] = 1;
  if (eng->actors->types[e] == ENTITY_TANK) {
    eng->actors->hitPoints[e] = 20;
  }
  if (eng->actors->types[e] == ENTITY_TANK_ALPHA) {
    eng->actors->hitPoints[e] = 500;
  }
  if (eng->actors->types[e] == ENTITY_HARASSER) {
    eng->actors->hitPoints[e] = 6;
  }

  // set position + prev position
  Vector3 *pos =
      (Vector3 *)getComponent(eng->actors, e, gs->compReg.cid_Positions);
  Vector3 *prev =
      (Vector3 *)getComponent(eng->actors, e, gs->compReg.cid_prevPositions);
  if (pos)
    *pos = worldPos;
  if (prev)
    *prev = worldPos;

  // insert into grid
  if (pos)
    GridAddEntity(&gs->grid, e, *pos);

  float *aiTimer =
      (float *)getComponent(eng->actors, e, gs->compReg.cid_aiTimer);
  if (aiTimer)
    *aiTimer = 0.0f;
}
