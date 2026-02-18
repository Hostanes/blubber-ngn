
#pragma once
#include "raylib.h"
#include <stdint.h>

typedef enum {
  MODEL_ROT_WORLD,
  MODEL_ROT_YAW_ONLY,
  MODEL_ROT_YAW_PITCH,
  MODEL_ROT_FULL,
} ModelRotationMode;

typedef struct {
  Model model;
  Vector3 offset;
  Vector3 rotation;
  Vector3 scale;
  ModelRotationMode rotationMode;

  Matrix finalTransform;
} ModelInstance_t;

typedef struct {
  uint32_t count;
  uint32_t capacity;
  ModelInstance_t *models;
} ModelCollection_t;

void ModelCollectionInit(ModelCollection_t *mc, uint32_t initialCapacity);
void ModelCollectionAdd(ModelCollection_t *mc, ModelInstance_t instance);
void ModelCollectionFree(ModelCollection_t *mc);
