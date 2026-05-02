#pragma once
#include "../engine/ecs/world.h"
#include "../engine/math/heightmap.h"
#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdbool.h>

typedef struct GameWorld GameWorld;

#define EDITOR_MAX_BOXES 512

typedef struct {
  entity_t entity;
  Vector3 position;
  Vector3 scale;
} EditorPlacedBox;

typedef struct {
  Camera3D camera;
  float yaw;
  float pitch;

  EditorPlacedBox placed[EDITOR_MAX_BOXES];
  int placedCount;

  float boxScale;
  Vector3 hitPos;
  bool hasHit;

  bool isNaming;
  char nameBuffer[128];
  int  nameLen;

  bool transformMode;  // false = place, true = select/transform
  int  selectedIndex;  // -1 = no selection

  bool isPaused;
  bool isSelectingLevel;
  bool requestQuit;
  char edLevelPaths[64][256];
  char edLevelNames[64][128];
  int  edLevelCount;
  int  edLevelSelected;

  bool initialized;
} EditorState;

void EditorInit(EditorState *ed, GameWorld *gw, world_t *world);
void EditorUpdate(EditorState *ed, GameWorld *gw, world_t *world);
void EditorRender(EditorState *ed, GameWorld *gw);
void EditorSave(EditorState *ed, const char *path);
void EditorLoad(EditorState *ed, GameWorld *gw, world_t *world, const char *path);
