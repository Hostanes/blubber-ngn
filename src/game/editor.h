#pragma once
#include "../engine/ecs/world.h"
#include "../engine/math/heightmap.h"
#include "game.h"
#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct GameWorld GameWorld;

#define EDITOR_MAX_BOXES      512
#define EDITOR_MAX_SPAWNERS   128
#define EDITOR_MAX_PROPS      256
#define EDITOR_MAX_INFOBOXES   64

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
  Vector3 position;
  float   halfExtent;
  char    message[256];
  float   duration;
  int     triggerCount;   // 0 = infinite
  float   markerHeight;   // Y offset of rotating model from box center
} EditorPlacedInfoBox;

typedef struct {
  Vector3 position;
  float   yaw;
  Vector3 scale;
  char    modelPath[256];
} EditorPlacedProp;

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
  int  selectedType;   // 0 = box, 1 = prop, 2 = info box (for transform mode)

  // Spawner placement
  EditorPlacedSpawner placedSpawners[EDITOR_MAX_SPAWNERS];
  int  spawnerCount;
  int  placeType;  // 0 = box, 1 = grunt spawner, 2 = ranger spawner, 3 = prop

  // Prop placement
  EditorPlacedProp placedProps[EDITOR_MAX_PROPS];
  int   propCount;
  float propPlaceYaw;   // yaw applied when placing props (scroll to rotate)
  float propPlaceScale; // uniform scale for prop placement

  // Object picker panel (replaces B/G/R keybinds)
  bool objectPanelOpen;

  // Model picker (for props + terrain)
  char edModelPaths[48][256]; // scanned from assets/models/
  char edModelNames[48][64];
  int  edModelCount;
  bool modelPickerOpen;       // M: pick prop model
  bool terrainPickerOpen;     // T: pick terrain model
  char propPlaceModel[256];   // model path for next placed prop
  int  modelPickerScroll;     // first visible item index

  // Info box placement
  EditorPlacedInfoBox placedInfoBoxes[EDITOR_MAX_INFOBOXES];
  int   infoBoxCount;
  float infoBoxHalfExtent;

  // Text input dialog (opens on info box placement)
  bool  infoBoxEditOpen;
  Vector3 infoBoxPendingPos;
  char  infoBoxTextBuf[256];
  int   infoBoxTextLen;
  float infoBoxDuration;
  int   infoBoxMaxTriggers;    // 0 = infinite
  float infoBoxMarkerHeight;   // Y offset of rotating model from box center
  bool  infoBoxEditExisting;   // true when dialog edits an already-placed box

  // Player spawn point
  Vector3 edSpawnPoint;
  bool    edHasSpawnPoint;

  // Undo history (0=box, 1=spawner, 2=prop, 4=infobox)
  uint8_t history[EDITOR_MAX_BOXES + EDITOR_MAX_SPAWNERS + EDITOR_MAX_PROPS + EDITOR_MAX_INFOBOXES];
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

  MissionType missionType;

  bool initialized;
} EditorState;

void EditorInit(EditorState *ed, GameWorld *gw, world_t *world);
void EditorUpdate(EditorState *ed, GameWorld *gw, world_t *world);
void EditorRender(EditorState *ed, GameWorld *gw);
void EditorSave(EditorState *ed, GameWorld *gw, const char *path);
void EditorLoad(EditorState *ed, GameWorld *gw, world_t *world, const char *path);
