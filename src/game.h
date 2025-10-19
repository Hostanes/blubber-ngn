// game.h
// ECS-style core game state and components

#pragma once
#include "raylib.h"
#include <stdbool.h>
#include <stdint.h>

#define MAX_ENTITIES 32 // can be increased later
#define TERRAIN_SIZE 100
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
} ModelCollection_t;

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
  C_PLAYER_TAG = 1u << 5,
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
