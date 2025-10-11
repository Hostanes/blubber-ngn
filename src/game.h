
// game.h
// Core game state and entity data structures

#pragma once
#include "raylib.h"
#include <stdbool.h>

#define MAX_ENTITIES 3

typedef enum {
  ENTITY_PLAYER,
  ENTITY_MECH,
  ENTITY_TANK,
  ENTITY_TURRET
} EntityType_t;

typedef enum { STATE_INLEVEL, STATE_MAINMENU } AllState_t;

// Orientation struct to group yaw, pitch, roll
typedef struct {
  float yaw;
  float pitch;
  float roll;
} Orientation;

// the center of the each collection is the position of the entity it belongs to
typedef struct {
  int countModels;
  Model *models;
  Vector3 *offsets; // offsets for each model with respect to the center
  Orientation *orientations;
  int *parentIds;
  bool **rotLocks; // 0 to lock orientation inheritance from parent, 1 to unlock
} ModelCollection_t;

typedef struct {
  int count;

  EntityType_t *types;

  Vector3 *positions;
  Vector3 *velocities;

  // Orientation *legOrientation;
  // Orientation *torsoOrientation;

  float *stepCycle;
  float *prevStepCycle;
  float *stepRate;

  ModelCollection_t *modelCollections;
} EntityData_t;

typedef struct {
  EntityData_t entities;
  int playerId;
  AllState_t state;
  float pHeadbobTimer;
} GameState_t;

GameState_t InitGame(void);
