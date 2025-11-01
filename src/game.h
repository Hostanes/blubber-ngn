// game.h
// ECS-style core game state and components

#pragma once
#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>

#define MAX_RAYS_PER_ENTITY 8
#define MAX_ENTITIES 256 // can be increased later
#define TERRAIN_SIZE 200
#define TERRAIN_SCALE 10.0f

//----------------------------------------
// Terrain
//----------------------------------------
typedef struct {
  float heights[TERRAIN_SIZE * TERRAIN_SIZE];
  Mesh mesh;
  Model model;
} Terrain_t;

//----------------------------------------
// Entity Types
//----------------------------------------
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
  int *parentIds;
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
  Vector3 velocities[MAX_ENTITIES];

  float stepCycle[MAX_ENTITIES];
  float prevStepCycle[MAX_ENTITIES];
  float stepRate[MAX_ENTITIES];

  ModelCollection_t modelCollections[MAX_ENTITIES];
  ModelCollection_t collisionCollections[MAX_ENTITIES];
  ModelCollection_t hitboxCollections[MAX_ENTITIES];

  Raycast_t raycasts[MAX_ENTITIES][MAX_RAYS_PER_ENTITY];
  int rayCounts[MAX_ENTITIES];

  float *cooldowns[MAX_ENTITIES]; // current cool down, can fire at 0
  float *firerate[MAX_ENTITIES];  // seconds between each shot

  float hitPoints[MAX_ENTITIES];

  EntityType_t types[MAX_ENTITIES];
} Components_t;

//----------------------------------------
// Game State
//----------------------------------------
typedef struct {
  EntityManager_t em;      // entity manager
  Components_t components; // all components

  int playerId;
  AllState_t state;
  float pHeadbobTimer;

  Terrain_t terrain;
} GameState_t;

//----------------------------------------
// Game Initialization
//----------------------------------------
GameState_t InitGame(void);
Vector3 ConvertOrientationToVector3(Orientation o);
