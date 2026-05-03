#pragma once
#include "../engine/ecs/world.h"
#include "../engine/math/heightmap.h"
#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct GameWorld GameWorld;

#define EDITOR_MAX_BOXES    512
#define EDITOR_MAX_SPAWNERS 128

typedef struct {
  entity_t entity;
  Vector3 position;
  Vector3 scale;
} EditorPlacedBox;

typedef struct {
  Vector3 position;
  int enemyType; // 0 = grunt, 1 = ranger
} EditorPlacedSpawner;

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

  // Spawner placement
  EditorPlacedSpawner placedSpawners[EDITOR_MAX_SPAWNERS];
  int  spawnerCount;
  int  placeType;  // 0 = box, 1 = grunt spawner, 2 = ranger spawner

  // Undo history (0=box, 1=spawner)
  uint8_t history[EDITOR_MAX_BOXES + EDITOR_MAX_SPAWNERS];
  int     historyTop;

  bool isPaused;
  bool isSelectingLevel;
  bool requestQuit;
  char edLevelPaths[64][256];
  char edLevelNames[64][128];
  int  edLevelCount;
  int  edLevelSelected;

  bool navPaintMode;
  int  navPaintType;    // 0=empty, 1=wall, 2=low_cover
  bool navPaletteOpen;
  int  navBrushSize;    // 1–5 cells (radius = brushSize-1)
  Image navImage;
  bool navImageLoaded;

  bool initialized;
} EditorState;

void EditorInit(EditorState *ed, GameWorld *gw, world_t *world);
void EditorUpdate(EditorState *ed, GameWorld *gw, world_t *world);
void EditorRender(EditorState *ed, GameWorld *gw);
void EditorSave(EditorState *ed, const char *path);
void EditorLoad(EditorState *ed, GameWorld *gw, world_t *world, const char *path);
