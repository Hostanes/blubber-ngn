
// game.h
// Core game state and entity data structures

#pragma once
#include "raylib.h"

#define MAX_ENTITIES 1

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

typedef struct {
  int count;

  EntityType_t *types;

  Vector3 *positions;
  Vector3 *velocities;

  Orientation *legOrientation;
  Orientation *torsoOrientation;

  float *stepCycle;
  float *prevStepCycle;
  float *stepRate;
} EntityData_t;

typedef struct {
  EntityData_t entities;
  int playerId;
  AllState_t state;
  float pHeadbobTimer;
} GameState_t;

GameState_t InitGame(void);
