#pragma once
#include "raylib.h"
#include "raymath.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

typedef enum {
  NAV_CELL_EMPTY = 0,
  NAV_CELL_WALL,
  NAV_CELL_COVER_LOW,
  NAV_CELL_COVER_HIGH,
  NAV_CELL_BLOCKED,
  NAV_CELL_SNIPE,   // ranger preferred firing position
  NAV_CELL_FLANK,   // grunt flanking approach path
  NAV_CELL_FENCE,   // impassable but LOS-transparent (shoot over, can't walk through)
} NavCellType;

typedef struct {
  Vector3 *points;
  int count;
  int capacity;
  int currentIndex;
} NavPath;

typedef struct {
  NavCellType type;
  uint8_t cost;
} NavCell;

typedef struct {
  int x, y;
  int gCost;
  int hCost;
  int fCost;
  int parentIndex;
  bool open;
  bool closed;
  int  heapIndex; // position in the open-set heap; -1 if not present
} AStarNode;

typedef struct {
  int width;
  int height;
  float cellSize;
  Vector3 origin;
  NavCell *cells;
} NavGrid;

static inline int NavGrid_Index(NavGrid *g, int x, int y) {
  return y * g->width + x;
}

static inline bool NavGrid_InBounds(NavGrid *g, int x, int y) {
  return x >= 0 && y >= 0 && x < g->width && y < g->height;
}

bool NavGrid_WorldToCell(NavGrid *g, Vector3 worldPos, int *outX, int *outY);
Vector3 NavGrid_CellCenter(NavGrid *g, int x, int y);
void NavGrid_SetCell(NavGrid *g, int x, int y, NavCellType type);
void NavGrid_Init(NavGrid *grid, int width, int height, float cellSize,
                  Vector3 origin);
void NavGrid_Destroy(NavGrid *grid);
void NavPath_Init(NavPath *path, int initialCapacity);
void NavPath_Clear(NavPath *path);
void NavPath_Destroy(NavPath *path);

bool NavGrid_FindPath(NavGrid *grid, Vector3 startWorld, Vector3 goalWorld,
                      NavPath *outPath);
bool NavGrid_LoadFromImage(NavGrid *grid, const char *fileName, float cellSize,
                           Vector3 origin);
bool NavGrid_HasLOS(NavGrid *g, Vector3 from, Vector3 to);
