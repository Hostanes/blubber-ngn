#pragma once
#include "raylib.h"
#include <stdint.h>

typedef uint32_t modelCollectionHandle_t;

enum {
  COMP_TRANSFORM = 0,
  COMP_MODEL_COLLECTION = 1,
};

typedef struct {
  Model model;     // from raylib
  Transform local; // offset relative to entity
} ModelInstance;

typedef struct {
  uint32_t count;
  ModelInstance *modelInstances;
} ModelCollection_t;

typedef struct {
  float time;
} Timer;

// helpers

static inline Transform TransformIdentity(void) {
  Transform t = {0};
  t.translation = (Vector3){0, 0, 0};
  t.rotation = (Quaternion){0, 0, 0, 1};
  t.scale = (Vector3){1, 1, 1};
  return t;
}
