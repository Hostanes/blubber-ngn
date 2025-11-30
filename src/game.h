// game.h
// ECS-style core game state and components

#pragma once
#include "engine.h"
#include "raylib.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

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
                              entity_t other);
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
  int entities[32];
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

  int cid_behavior;
  // TODO add others
} ActorComponentRegistry_t;

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
} GameState_t;

//----------------------------------------
// Game Initialization
//----------------------------------------
GameState_t InitGame(Engine_t *eng);
Vector3 ConvertOrientationToVector3(Orientation o);

//----------------------------------------
// Grid Initialization
//----------------------------------------
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

static inline void GridAddEntity(EntityGrid_t *grid, entity_t e, Vector3 pos) {
  int ix = (int)((pos.x - grid->minX) / grid->cellSize);
  int iz = (int)((pos.z - grid->minZ) / grid->cellSize);

  if (!IsCellValid(grid, ix, iz))
    return;

  GridNode_t *node = &grid->nodes[ix][iz];
  if (node->count < MAX_GRID_NODES) {
    node->entities[node->count++] = e;
  }
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
  EntityType_t type = eng->actors.types[idx];

  if (type != ENTITY_PLAYER && type != ENTITY_MECH && type != ENTITY_TANK)
    return;

  Vector3 *prevPos =
      (Vector3 *)getComponent(&eng->actors, e, gs->compReg.cid_prevPositions);

  Vector3 *currPos =
      (Vector3 *)getComponent(&eng->actors, e, gs->compReg.cid_Positions);

  GridRemoveEntity(&gs->grid, e, *prevPos);

  GridAddEntity(&gs->grid, e, *currPos);

  prevPos = currPos;
}

//----------------------------------------
// Iterate over entities in a cell
//----------------------------------------
#define FOR_EACH_ENTITY_IN_CELL(cell, entityVar)                               \
  for (int _i = 0;                                                             \
       _i < (cell)->count && (((entityVar) = (cell)->entities[_i]) || 1);      \
       _i++)
