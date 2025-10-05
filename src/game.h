
// game.h
// Core game state and entity data structures

#pragma once
#include "raylib.h"

#define MAX_ENTITIES 1

// Orientation struct to group yaw, pitch, roll
typedef struct {
  float yaw;
  float pitch;
  float roll;
} Orientation;

typedef struct {
  int count;
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
  float pHeadbobTimer;
} GameState_t;

GameState_t InitGame(void);
