#include "nav.h"

void NavGrid_Init(NavGrid *grid, int width, int height, float cellSize,
                  Vector3 origin) {
  grid->width = width;
  grid->height = height;
  grid->cellSize = cellSize;
  grid->origin = origin;

  grid->cells = malloc(sizeof(NavCell) * width * height);

  for (int i = 0; i < width * height; i++) {
    grid->cells[i].type = NAV_CELL_EMPTY;
    grid->cells[i].cost = 1;
  }
}

void NavGrid_Destroy(NavGrid *grid) { free(grid->cells); }

// takes world position and return grid indicies
bool NavGrid_WorldToCell(NavGrid *g, Vector3 worldPos, int *outX, int *outY) {
  float localX = worldPos.x - g->origin.x;
  float localZ = worldPos.z - g->origin.z;

  int x = (int)(localX / g->cellSize);
  int y = (int)(localZ / g->cellSize);

  if (!NavGrid_InBounds(g, x, y))
    return false;

  *outX = x;
  *outY = y;
  return true;
}

Vector3 NavGrid_CellCenter(NavGrid *g, int x, int y) {
  return (Vector3){g->origin.x + x * g->cellSize + g->cellSize * 0.5f, 0,
                   g->origin.z + y * g->cellSize + g->cellSize * 0.5f};
}

void NavGrid_SetCell(NavGrid *g, int x, int y, NavCellType type) {
  if (!NavGrid_InBounds(g, x, y))
    return;

  g->cells[NavGrid_Index(g, x, y)].type = type;

  if (type == NAV_CELL_WALL)
    g->cells[NavGrid_Index(g, x, y)].cost = 255;
}
