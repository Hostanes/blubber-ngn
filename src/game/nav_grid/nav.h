#pragma once
#include "raylib.h"
#include "raymath.h"
#include <stdint.h>
#include <stdlib.h>

typedef enum {
  NAV_CELL_EMPTY = 0,
  NAV_CELL_WALL,
  NAV_CELL_COVER_LOW,
  NAV_CELL_COVER_HIGH,
  NAV_CELL_BLOCKED
} NavCellType;

typedef struct {
  NavCellType type;
  uint8_t cost;
} NavCell;

typedef struct {
  int width;
  int height;
  float cellSize;
  Vector3 origin; // world-space bottom-left corner
  NavCell *cells; // width * height
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
