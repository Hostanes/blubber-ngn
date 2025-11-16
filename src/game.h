// game.h
// ECS-style core game state and components

#pragma once
#include "raylib.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#define HEIGHTMAP_RES_X 512
#define HEIGHTMAP_RES_Z 512

#define MAX_RAYS_PER_ENTITY 8
#define TERRAIN_SIZE 200
#define TERRAIN_SCALE 10.0f

#define MAX_ENTITIES 256 // can be increased later
#define MAX_STATICS 256
#define MAX_PROJECTILES 1024
#define MAX_PARTICLES 2048

#define ENTITY_TYPE_SHIFT 30
#define ENTITY_INDEX_MASK 0x3FFFFFFF

//----------------------------------------
// Terrain
//----------------------------------------
typedef struct {
  Mesh mesh;
  Model model;

  float *height; // 2d array of height info stored as 1D

  float minX;
  float minZ;

  int hmWidth;
  int hmHeight;

  float cellSizeX;
  float cellSizeZ;

  float worldWidth;
  float worldLength;
} Terrain_t;

//----------------------------------------
// Entity Types
//----------------------------------------

typedef enum {
  ET_ACTOR = 0,
  ET_STATIC = 1,
  ET_PROJECTILE = 2,
} EntityCategory_t;

typedef enum {
  ENTITY_PLAYER,
  ENTITY_MECH,
  ENTITY_TANK,
  ENTITY_WALL,
  ENTITY_TURRET
} EntityType_t;

typedef enum { STATE_INLEVEL, STATE_MAINMENU } AllState_t;

//----------------------------------------
// Orientation
//----------------------------------------
typedef struct {
  float yaw;
  float pitch;
  float roll;
} Orientation;

//----------------------------------------
// Model Collections
//----------------------------------------
typedef struct {
  int countModels;
  Model *models;
  Vector3 *offsets;
  Orientation *orientations;
  int *parentIds; //  TODO parents should be specific for each axis
  bool **rotLocks;

  Orientation *localRotationOffset;
  bool **rotInverts;

  // World-space transform (computed each frame)
  Vector3 *globalPositions;
  Orientation *globalOrientations;
} ModelCollection_t;

typedef struct {
  bool active;

  int parentModelIndex;  // which visual model to attach to
  Vector3 localOffset;   // offset from model origin
  Orientation oriOffset; // direction local to model

  Ray ray; // holds startpoint and orientation data
  float distance;
} Raycast_t;

//----------------------------------------
// ECS: Component Flags
//----------------------------------------
typedef uint32_t ComponentMask_t;

typedef enum {
  C_NONE = 0,
  C_POSITION = 1u << 0,
  C_VELOCITY = 1u << 1,
  C_MODEL = 1u << 2,
  C_COLLISION = 1u << 3,
  C_HITBOX = 1u << 4,
  C_RAYCAST = 1u << 5,
  C_PLAYER_TAG = 1u << 6,
  C_COOLDOWN_TAG = 1u << 7,
  C_HITPOINT_TAG = 1u << 8,
  C_TURRET_BEHAVIOUR_1 = 1u << 9,
  C_GRAVITY = 1u << 10,
} ComponentFlag_t;

//----------------------------------------
// ECS: Entity Manager
//----------------------------------------
typedef int32_t entity_t;

typedef struct {
  uint8_t alive[MAX_ENTITIES];
  ComponentMask_t masks[MAX_ENTITIES];
  int count;
} EntityManager_t;

//----------------------------------------
// ECS: Components
//----------------------------------------
typedef struct {
  Vector3 positions[MAX_ENTITIES];
  Vector3 prevPositions[MAX_ENTITIES];
  Vector3 velocities[MAX_ENTITIES];

  float stepCycle[MAX_ENTITIES];
  float prevStepCycle[MAX_ENTITIES];
  float stepRate[MAX_ENTITIES];

  ModelCollection_t modelCollections[MAX_ENTITIES];
  ModelCollection_t collisionCollections[MAX_ENTITIES];
  ModelCollection_t hitboxCollections[MAX_ENTITIES];

  Raycast_t raycasts[MAX_ENTITIES][MAX_RAYS_PER_ENTITY];
  int rayCounts[MAX_ENTITIES];

  // ===== weapons =====
  float *firerate[MAX_ENTITIES];         // seconds between each shot
  float *cooldowns[MAX_ENTITIES];        // current cool down, can fire at 0
  float *dropRates[MAX_ENTITIES];        // vertical drop rate for projectiles
  float *muzzleVelocities[MAX_ENTITIES]; // speed of proj upon exiting barrel

  float hitPoints[MAX_ENTITIES];

  EntityType_t types[MAX_ENTITIES];
} ActorComponents_t;

typedef struct {
  bool active[MAX_PROJECTILES];

  Vector3 positions[MAX_PROJECTILES];
  Vector3 velocities[MAX_PROJECTILES];

  float dropRates[MAX_PROJECTILES]; // vertical drop rate for projectiles
  float lifetimes[MAX_PROJECTILES]; // count down to 0
  float radii[MAX_PROJECTILES];     // for circular hitbox

  entity_t owners[MAX_PROJECTILES]; // which entity shot it

  int types[MAX_PROJECTILES];

} ProjectilePool_t;

typedef struct {
  Vector3 positions[MAX_STATICS];

  ModelCollection_t modelCollections[MAX_STATICS];
  ModelCollection_t collisionCollections[MAX_STATICS];
  ModelCollection_t hitboxCollections[MAX_STATICS];

} StaticPool_t;

typedef struct {

  int types[MAX_PARTICLES];
  bool active[MAX_PARTICLES];

  Vector3 positions[MAX_PARTICLES];
  float lifetimes[MAX_PARTICLES];
  float startLifetimes[MAX_PARTICLES];

} ParticlePool_t;

//----------------------------------------
// Entity Grid
//----------------------------------------

#define GRID_CELL_SIZE 100.0f
#define MAX_GRID_NODES 32
#define GRID_EMPTY -1

typedef struct {
  int entities[MAX_GRID_NODES]; // entity IDs in this cell
  int count;
} GridNode_t;

typedef struct {
  GridNode_t **nodes; // dynamic 2D array, malloced at init
  float cellSize;
  float minX, minZ;  // origin of the grid (corner)
  int width, length; // number of cells along X and Z
} EntityGrid_t;

//----------------------------------------
// Game State
//----------------------------------------
typedef struct {
  EntityManager_t em;           // entity manager
  ActorComponents_t components; // all components

  ProjectilePool_t projectiles;

  StaticPool_t statics;

  ParticlePool_t particles;

  int playerId;
  AllState_t state;
  float pHeadbobTimer;

  Terrain_t terrain;

  EntityGrid_t grid;
} GameState_t;

//----------------------------------------
// Game Initialization
//----------------------------------------
GameState_t InitGame(void);
Vector3 ConvertOrientationToVector3(Orientation o);

// Inline category ID helpers
static inline entity_t MakeEntityID(EntityCategory_t cat, int index) {
  return ((uint32_t)cat << ENTITY_TYPE_SHIFT) | (index & ENTITY_INDEX_MASK);
}

static inline EntityCategory_t GetEntityCategory(entity_t id) {
  return (EntityCategory_t)(id >> ENTITY_TYPE_SHIFT);
}

static inline int GetEntityIndex(entity_t id) { return id & ENTITY_INDEX_MASK; }

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

static inline void UpdateEntityInGrid(GameState_t *gs, entity_t e) {
  int idx = GetEntityIndex(e);
  EntityType_t type = gs->components.types[idx];

  if (type != ENTITY_PLAYER && type != ENTITY_MECH && type != ENTITY_TANK)
    return;

  Vector3 prevPos = gs->components.prevPositions[idx];
  Vector3 currPos = gs->components.positions[idx];

  GridRemoveEntity(&gs->grid, e, prevPos);

  GridAddEntity(&gs->grid, e, currPos);

  gs->components.prevPositions[idx] = currPos;
}

//----------------------------------------
// Iterate over entities in a cell
//----------------------------------------
#define FOR_EACH_ENTITY_IN_CELL(cell, entityVar)                               \
  for (int _i = 0;                                                             \
       _i < (cell)->count && (((entityVar) = (cell)->entities[_i]) || 1);      \
       _i++)
