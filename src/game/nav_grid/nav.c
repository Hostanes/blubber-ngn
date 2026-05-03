#include "nav.h"
#include <string.h>

void NavGrid_Init(NavGrid *grid, int width, int height, float cellSize,
                  Vector3 origin) {
  grid->width    = width;
  grid->height   = height;
  grid->cellSize = cellSize;
  grid->origin   = origin;
  grid->cells    = malloc(sizeof(NavCell) * width * height);
  for (int i = 0; i < width * height; i++) {
    grid->cells[i].type = NAV_CELL_EMPTY;
    grid->cells[i].cost = 1;
  }
}

void NavGrid_Destroy(NavGrid *grid) { free(grid->cells); }

bool NavGrid_WorldToCell(NavGrid *g, Vector3 worldPos, int *outX, int *outY) {
  float localX = worldPos.x - g->origin.x;
  float localZ = worldPos.z - g->origin.z;
  int x = (int)(localX / g->cellSize);
  int y = (int)(localZ / g->cellSize);
  if (!NavGrid_InBounds(g, x, y)) return false;
  *outX = x;
  *outY = y;
  return true;
}

Vector3 NavGrid_CellCenter(NavGrid *g, int x, int y) {
  return (Vector3){g->origin.x + x * g->cellSize + g->cellSize * 0.5f, 0,
                   g->origin.z + y * g->cellSize + g->cellSize * 0.5f};
}

void NavGrid_SetCell(NavGrid *g, int x, int y, NavCellType type) {
  if (!NavGrid_InBounds(g, x, y)) return;
  int idx = NavGrid_Index(g, x, y);
  g->cells[idx].type = type;
  if (type == NAV_CELL_WALL) g->cells[idx].cost = 255;
}

void NavPath_Init(NavPath *path, int initialCapacity) {
  path->count        = 0;
  path->capacity     = initialCapacity;
  path->points       = malloc(sizeof(Vector3) * initialCapacity);
  path->currentIndex = 0;
}

void NavPath_Clear(NavPath *path) {
  path->count        = 0;
  path->currentIndex = 0;
}

void NavPath_Destroy(NavPath *path) {
  free(path->points);
  path->points = NULL;
}

/* ------------------------------------------------------------------ */
/*  A* — static buffers (no per-call malloc)                          */
/* ------------------------------------------------------------------ */

#define NAV_MAX_CELLS (180 * 180)

static AStarNode s_nodes[NAV_MAX_CELLS];
static int       s_heap[NAV_MAX_CELLS]; // stores node indices
static int       s_heapSize;

static inline int Heuristic(int x1, int y1, int x2, int y2) {
  int dx = abs(x1 - x2);
  int dy = abs(y1 - y2);
  return 10 * (dx + dy) + (14 - 20) * (dx < dy ? dx : dy);
}

/* Binary min-heap (keyed on fCost, break ties with hCost) */

static inline bool HeapLess(int a, int b) {
  if (s_nodes[a].fCost != s_nodes[b].fCost)
    return s_nodes[a].fCost < s_nodes[b].fCost;
  return s_nodes[a].hCost < s_nodes[b].hCost;
}

static void HeapSiftUp(int pos) {
  while (pos > 0) {
    int parent = (pos - 1) / 2;
    if (HeapLess(s_heap[pos], s_heap[parent])) {
      int tmp       = s_heap[pos];
      s_heap[pos]   = s_heap[parent];
      s_heap[parent]= tmp;
      s_nodes[s_heap[pos]].heapIndex    = pos;
      s_nodes[s_heap[parent]].heapIndex = parent;
      pos = parent;
    } else break;
  }
}

static void HeapSiftDown(int pos) {
  while (true) {
    int left  = 2 * pos + 1;
    int right = 2 * pos + 2;
    int best  = pos;
    if (left  < s_heapSize && HeapLess(s_heap[left],  s_heap[best])) best = left;
    if (right < s_heapSize && HeapLess(s_heap[right], s_heap[best])) best = right;
    if (best == pos) break;
    int tmp        = s_heap[pos];
    s_heap[pos]    = s_heap[best];
    s_heap[best]   = tmp;
    s_nodes[s_heap[pos]].heapIndex  = pos;
    s_nodes[s_heap[best]].heapIndex = best;
    pos = best;
  }
}

static void HeapPush(int nodeIdx) {
  s_heap[s_heapSize]               = nodeIdx;
  s_nodes[nodeIdx].heapIndex       = s_heapSize;
  s_heapSize++;
  HeapSiftUp(s_heapSize - 1);
}

static int HeapPop(void) {
  int top = s_heap[0];
  s_heapSize--;
  if (s_heapSize > 0) {
    s_heap[0]                         = s_heap[s_heapSize];
    s_nodes[s_heap[0]].heapIndex      = 0;
    HeapSiftDown(0);
  }
  s_nodes[top].heapIndex = -1;
  return top;
}

// Call after updating node's fCost downward.
static void HeapDecrease(int nodeIdx) {
  int pos = s_nodes[nodeIdx].heapIndex;
  if (pos >= 0) HeapSiftUp(pos);
}

bool NavGrid_FindPath(NavGrid *grid, Vector3 startWorld, Vector3 goalWorld,
                      NavPath *outPath) {
  outPath->count        = 0;
  outPath->currentIndex = 0;

  int startX, startY, goalX, goalY;
  if (!NavGrid_WorldToCell(grid, startWorld, &startX, &startY)) return false;
  if (!NavGrid_WorldToCell(grid, goalWorld,  &goalX,  &goalY))  return false;

  int total = grid->width * grid->height;

  // Initialise node buffer for this search
  for (int y = 0; y < grid->height; y++) {
    for (int x = 0; x < grid->width; x++) {
      int idx           = NavGrid_Index(grid, x, y);
      s_nodes[idx].x           = x;
      s_nodes[idx].y           = y;
      s_nodes[idx].gCost       = 999999;
      s_nodes[idx].hCost       = 0;
      s_nodes[idx].fCost       = 999999;
      s_nodes[idx].parentIndex = -1;
      s_nodes[idx].open        = false;
      s_nodes[idx].closed      = false;
      s_nodes[idx].heapIndex   = -1;
    }
  }

  s_heapSize = 0;

  int startIndex = NavGrid_Index(grid, startX, startY);
  int goalIndex  = NavGrid_Index(grid, goalX,  goalY);

  s_nodes[startIndex].gCost = 0;
  s_nodes[startIndex].hCost = Heuristic(startX, startY, goalX, goalY);
  s_nodes[startIndex].fCost = s_nodes[startIndex].hCost;
  s_nodes[startIndex].open  = true;
  HeapPush(startIndex);

  const int dirs[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{-1,1},{1,-1},{-1,-1}};

  while (s_heapSize > 0) {
    int currentIndex = HeapPop();
    if (currentIndex == goalIndex) break;

    s_nodes[currentIndex].open   = false;
    s_nodes[currentIndex].closed = true;

    int cx = s_nodes[currentIndex].x;
    int cy = s_nodes[currentIndex].y;

    for (int i = 0; i < 8; i++) {
      int nx = cx + dirs[i][0];
      int ny = cy + dirs[i][1];
      if (!NavGrid_InBounds(grid, nx, ny)) continue;

      int neighborIndex = NavGrid_Index(grid, nx, ny);
      if (grid->cells[neighborIndex].type == NAV_CELL_WALL) continue;
      if (s_nodes[neighborIndex].closed) continue;

      bool isDiagonal = (dirs[i][0] != 0 && dirs[i][1] != 0);
      if (isDiagonal) {
        int adj1 = NavGrid_Index(grid, cx + dirs[i][0], cy);
        int adj2 = NavGrid_Index(grid, cx, cy + dirs[i][1]);
        if (grid->cells[adj1].type == NAV_CELL_WALL ||
            grid->cells[adj2].type == NAV_CELL_WALL) continue;
      }

      int moveCost = isDiagonal ? 14 : 10;
      int newG     = s_nodes[currentIndex].gCost + moveCost
                     + grid->cells[neighborIndex].cost;

      if (!s_nodes[neighborIndex].open) {
        s_nodes[neighborIndex].gCost       = newG;
        s_nodes[neighborIndex].hCost       = Heuristic(nx, ny, goalX, goalY);
        s_nodes[neighborIndex].fCost       = newG + s_nodes[neighborIndex].hCost;
        s_nodes[neighborIndex].parentIndex = currentIndex;
        s_nodes[neighborIndex].open        = true;
        HeapPush(neighborIndex);
      } else if (newG < s_nodes[neighborIndex].gCost) {
        s_nodes[neighborIndex].gCost       = newG;
        s_nodes[neighborIndex].fCost       = newG + s_nodes[neighborIndex].hCost;
        s_nodes[neighborIndex].parentIndex = currentIndex;
        HeapDecrease(neighborIndex);
      }
    }
  }

  if (s_nodes[goalIndex].parentIndex == -1 && goalIndex != startIndex)
    return false;

  // Reconstruct path
  NavPath_Clear(outPath);
  int current = goalIndex;
  while (current != -1) {
    if (outPath->count >= outPath->capacity) {
      outPath->capacity *= 2;
      outPath->points = realloc(outPath->points,
                                sizeof(Vector3) * outPath->capacity);
    }
    outPath->points[outPath->count++] =
        NavGrid_CellCenter(grid, s_nodes[current].x, s_nodes[current].y);
    current = s_nodes[current].parentIndex;
  }

  // Reverse to [start ... goal]
  for (int i = 0; i < outPath->count / 2; i++) {
    Vector3 tmp                             = outPath->points[i];
    outPath->points[i]                      = outPath->points[outPath->count-1-i];
    outPath->points[outPath->count - 1 - i] = tmp;
  }

  return true;
}

/*
  White (255,255,255) NAV_CELL_EMPTY
  Black (0,0,0)       NAV_CELL_WALL
  Blue  (0,0,255)     NAV_CELL_COVER_LOW
  Green (0,255,0)     NAV_CELL_COVER_HIGH
  Red   (255,0,0)     NAV_CELL_BLOCKED
*/
bool NavGrid_LoadFromImage(NavGrid *grid, const char *fileName, float cellSize,
                           Vector3 origin) {
  Image img = LoadImage(fileName);
  if (img.data == NULL) return false;

  int width  = img.width;
  int height = img.height;
  NavGrid_Init(grid, width, height, cellSize, origin);

  Color *pixels = LoadImageColors(img);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      int flippedY = height - 1 - y;
      Color c = pixels[y * width + x];

      NavCellType type = NAV_CELL_EMPTY;
      uint8_t     cost = 1;

      if      (c.r == 0   && c.g == 0   && c.b == 0)   { type = NAV_CELL_WALL;       cost = 255; }
      else if (c.r == 0   && c.g == 0   && c.b == 255) { type = NAV_CELL_COVER_LOW;  cost = 2;   }
      else if (c.r == 0   && c.g == 255 && c.b == 0)   { type = NAV_CELL_COVER_HIGH; cost = 3;   }
      else if (c.r == 255 && c.g == 0   && c.b == 0)   { type = NAV_CELL_BLOCKED;    cost = 255; }

      int idx = NavGrid_Index(grid, x, flippedY);
      grid->cells[idx].type = type;
      grid->cells[idx].cost = cost;
    }
  }

  UnloadImageColors(pixels);
  UnloadImage(img);
  return true;
}
