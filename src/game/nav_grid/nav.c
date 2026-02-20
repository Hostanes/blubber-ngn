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

void NavPath_Init(NavPath *path, int initialCapacity) {
  path->count = 0;
  path->capacity = initialCapacity;
  path->points = malloc(sizeof(Vector3) * initialCapacity);
  path->currentIndex = 0;
}

void NavPath_Clear(NavPath *path) {
  path->count = 0;
  path->currentIndex = 0;
}

void NavPath_Destroy(NavPath *path) {
  free(path->points);
  path->points = NULL;
}

static int Heuristic(int x1, int y1, int x2, int y2) {
  int dx = abs(x1 - x2);
  int dy = abs(y1 - y2);

  int D = 10;
  int D2 = 14; // diagonal cost ≈ sqrt(2)*10

  return D * (dx + dy) + (D2 - 2 * D) * (dx < dy ? dx : dy);
}

static int GetLowestFCost(AStarNode *nodes, int total) {
  int best = -1;

  for (int i = 0; i < total; i++) {
    if (!nodes[i].open)
      continue;

    if (best == -1 || nodes[i].fCost < nodes[best].fCost)
      best = i;
  }

  return best;
}

bool NavGrid_FindPath(NavGrid *grid, Vector3 startWorld, Vector3 goalWorld,
                      NavPath *outPath) {
  int startX, startY;
  int goalX, goalY;

  if (!NavGrid_WorldToCell(grid, startWorld, &startX, &startY))
    return false;

  if (!NavGrid_WorldToCell(grid, goalWorld, &goalX, &goalY))
    return false;

  int total = grid->width * grid->height;

  AStarNode *nodes = malloc(sizeof(AStarNode) * total);

  // Initialize nodes
  for (int y = 0; y < grid->height; y++) {
    for (int x = 0; x < grid->width; x++) {
      int idx = NavGrid_Index(grid, x, y);

      nodes[idx].x = x;
      nodes[idx].y = y;
      nodes[idx].gCost = 999999;
      nodes[idx].hCost = 0;
      nodes[idx].fCost = 999999;
      nodes[idx].parentIndex = -1;
      nodes[idx].open = false;
      nodes[idx].closed = false;
    }
  }

  int startIndex = NavGrid_Index(grid, startX, startY);
  int goalIndex = NavGrid_Index(grid, goalX, goalY);

  nodes[startIndex].gCost = 0;
  nodes[startIndex].hCost = Heuristic(startX, startY, goalX, goalY);
  nodes[startIndex].fCost = nodes[startIndex].hCost;
  nodes[startIndex].open = true;

  // 8 directions
  const int dirs[8][2] = {{1, 0}, {-1, 0}, {0, 1},  {0, -1},
                          {1, 1}, {-1, 1}, {1, -1}, {-1, -1}};

  while (true) {
    int currentIndex = GetLowestFCost(nodes, total);
    if (currentIndex == -1)
      break;

    if (currentIndex == goalIndex)
      break;

    nodes[currentIndex].open = false;
    nodes[currentIndex].closed = true;

    int cx = nodes[currentIndex].x;
    int cy = nodes[currentIndex].y;

    for (int i = 0; i < 8; i++) {
      int nx = cx + dirs[i][0];
      int ny = cy + dirs[i][1];

      if (!NavGrid_InBounds(grid, nx, ny))
        continue;

      int neighborIndex = NavGrid_Index(grid, nx, ny);

      if (grid->cells[neighborIndex].type == NAV_CELL_WALL)
        continue;

      if (nodes[neighborIndex].closed)
        continue;

      // Prevent diagonal corner cutting
      bool isDiagonal = (dirs[i][0] != 0 && dirs[i][1] != 0);

      if (isDiagonal) {
        int adj1 = NavGrid_Index(grid, cx + dirs[i][0], cy);
        int adj2 = NavGrid_Index(grid, cx, cy + dirs[i][1]);

        if (grid->cells[adj1].type == NAV_CELL_WALL ||
            grid->cells[adj2].type == NAV_CELL_WALL)
          continue;
      }

      int moveCost = (isDiagonal ? 14 : 10);
      int newG = nodes[currentIndex].gCost + moveCost +
                 grid->cells[neighborIndex].cost;

      if (!nodes[neighborIndex].open || newG < nodes[neighborIndex].gCost) {
        nodes[neighborIndex].gCost = newG;
        nodes[neighborIndex].hCost = Heuristic(nx, ny, goalX, goalY);
        nodes[neighborIndex].fCost =
            nodes[neighborIndex].gCost + nodes[neighborIndex].hCost;

        nodes[neighborIndex].parentIndex = currentIndex;
        nodes[neighborIndex].open = true;
      }
    }
  }

  // If goal unreachable
  if (nodes[goalIndex].parentIndex == -1 && goalIndex != startIndex) {
    free(nodes);
    return false;
  }

  // Reconstruct path (reverse order)
  NavPath_Clear(outPath);

  int current = goalIndex;

  while (current != -1) {
    AStarNode *n = &nodes[current];

    if (outPath->count >= outPath->capacity) {
      outPath->capacity *= 2;
      outPath->points =
          realloc(outPath->points, sizeof(Vector3) * outPath->capacity);
    }

    outPath->points[outPath->count++] = NavGrid_CellCenter(grid, n->x, n->y);

    current = n->parentIndex;
  }

  free(nodes);
  return true;
}
