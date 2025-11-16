
#include "game.h"
#include <stdlib.h>

// Call when creating the game / grid
bool Grid_Init(EntityGrid_t *g, float worldMinX, float worldMinZ, int cellsX,
               int cellsZ, float cellSize) {
  int totalCells = cellsX * cellsZ;
  g->cellsX = cellsX;
  g->cellsZ = cellsZ;
  g->cellSize = cellSize;
  g->worldMinX = worldMinX;
  g->worldMinZ = worldMinZ;

  g->cellHeads = (int *)malloc(sizeof(int) * totalCells);
  if (!g->cellHeads)
    return false;
  for (int i = 0; i < totalCells; ++i)
    g->cellHeads[i] = GRID_EMPTY;

  g->nodes = (GridNode_t *)malloc(sizeof(GridNode_t) * MAX_GRID_NODES);
  if (!g->nodes) {
    free(g->cellHeads);
    return false;
  }

  // build freelist
  for (int i = 0; i < MAX_GRID_NODES - 1; ++i)
    g->nodes[i].next = i + 1;
  g->nodes[MAX_GRID_NODES - 1].next = GRID_EMPTY;
  g->freeList = 0;

  // mark all entities as not-in-grid
  for (int i = 0; i < MAX_ENTITIES; ++i)
    g->nodeForEntity[i] = GRID_EMPTY;

  return true;
}

void Grid_Destroy(EntityGrid_t *g) {
  if (g->cellHeads)
    free(g->cellHeads);
  if (g->nodes)
    free(g->nodes);
  g->cellHeads = NULL;
  g->nodes = NULL;
  g->freeList = GRID_EMPTY;
}

static inline int Grid_AllocNode(EntityGrid_t *g) {
  int node = g->freeList;
  if (node == GRID_EMPTY)
    return GRID_EMPTY; // out of nodes!
  g->freeList = g->nodes[node].next;
  return node;
}

static inline void Grid_FreeNode(EntityGrid_t *g, int node) {
  g->nodes[node].next = g->freeList;
  g->freeList = node;
}

// Insert actor (actorIndex is 0..MAX_ENTITIES-1) at world position pos.
// Returns true on success, false if out-of-grid or out-of-nodes.
bool Grid_InsertEntity(EntityGrid_t *g, entity_t ent, int actorIndex,
                       Vector3 pos) {
  int cell = Grid_GetCellIndex(g, pos);
  if (cell < 0)
    return false;

  // If already in some node, remove it first (we could instead move if same
  // cell)
  if (g->nodeForEntity[actorIndex] != GRID_EMPTY) {
    // If already in same cell, nothing to do (optional optimization)
    int currentNode = g->nodeForEntity[actorIndex];
    // find which cell head points to this node would require searching
    // simpler: just remove node by scanning that cell's list (we store cell
    // index by pos above), but since we only guarantee a single cell, better to
    // call Grid_RemoveEntity below. For simplicity here call Grid_RemoveEntity:
    // (we'll implement Grid_RemoveEntity next)
    // NOTE: caller could check and call Grid_MoveEntity instead to avoid double
    // work. We'll remove here: (implementation expects
    // Grid_RemoveEntity(EntityGrid_t*, actorIndex) present) For now, assume
    // function exists:
    extern void Grid_RemoveEntityByActorIndex(EntityGrid_t * g, int actorIndex);
    Grid_RemoveEntityByActorIndex(g, actorIndex);
  }

  int node = Grid_AllocNode(g);
  if (node == GRID_EMPTY)
    return false; // out of pool

  g->nodes[node].entity = ent;
  // push front
  g->nodes[node].next = g->cellHeads[cell];
  g->cellHeads[cell] = node;

  g->nodeForEntity[actorIndex] = node;
  return true;
}
