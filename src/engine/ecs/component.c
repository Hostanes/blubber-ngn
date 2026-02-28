#include "component.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void ComponentPoolInit(componentPool_t *componentPool, size_t elementSize) {
  componentPool->denseHandles = NULL;
  componentPool->denseData = NULL;
  componentPool->sparse = NULL;

  componentPool->count = 0;
  componentPool->denseCapacity = 0;
  componentPool->sparseCapacity = 0;
  componentPool->nextHandle = 0;
  componentPool->elementSize = elementSize;

  componentPool->freeHandles = NULL;
  componentPool->freeCount = 0;
  componentPool->freeCapacity = 0;
}

static void ComponentPoolGrow(componentPool_t *componentPool) {
  uint32_t oldCapacity = componentPool->denseCapacity;
  uint32_t newCapacity = oldCapacity == 0 ? 64 : oldCapacity * 2;

  componentPool->denseHandles =
      realloc(componentPool->denseHandles, newCapacity * sizeof(uint32_t));

  componentPool->denseData = realloc(componentPool->denseData,
                                     newCapacity * componentPool->elementSize);

  componentPool->denseCapacity = newCapacity;
}

static void ComponentPoolEnsureSparse(componentPool_t *pool, uint32_t handle) {
  uint32_t required = handle + 1;

  if (required <= pool->sparseCapacity)
    return;

  uint32_t oldCap = pool->sparseCapacity;
  uint32_t newCap = oldCap == 0 ? 64 : oldCap;

  while (newCap < required)
    newCap *= 2;

  pool->sparse = realloc(pool->sparse, newCap * sizeof(uint32_t));

  for (uint32_t i = oldCap; i < newCap; ++i)
    pool->sparse[i] = UINT32_MAX;

  pool->sparseCapacity = newCap;
}

static void ComponentPoolGrowFreeList(componentPool_t *pool) {
  uint32_t newCap = pool->freeCapacity == 0 ? 64 : pool->freeCapacity * 2;

  pool->freeHandles = realloc(pool->freeHandles, newCap * sizeof(uint32_t));

  pool->freeCapacity = newCap;
}

void ComponentRemove(componentPool_t *componentPool, uint32_t handle) {

  if (!ComponentHas(componentPool, handle))
    return;

  uint32_t index = componentPool->sparse[handle];
  uint32_t last = componentPool->count - 1;

  if (index != last) {
    componentPool->denseHandles[index] = componentPool->denseHandles[last];

    memcpy((char *)componentPool->denseData +
               index * componentPool->elementSize,
           (char *)componentPool->denseData + last * componentPool->elementSize,
           componentPool->elementSize);

    uint32_t moved = componentPool->denseHandles[index];
    componentPool->sparse[moved] = index;
  }

  componentPool->sparse[handle] = UINT32_MAX;
  componentPool->count--;

  // ---- add handle to free list ----
  if (componentPool->freeCount >= componentPool->freeCapacity)
    ComponentPoolGrowFreeList(componentPool);

  componentPool->freeHandles[componentPool->freeCount++] = handle;
}

void *ComponentGet(componentPool_t *componentPool, uint32_t handle) {
  if (!ComponentHas(componentPool, handle))
    return NULL;

  uint32_t index = componentPool->sparse[handle];
  return (char *)componentPool->denseData + index * componentPool->elementSize;
}

bool ComponentHas(const componentPool_t *componentPool, uint32_t handle) {
  if (!componentPool->sparse || handle >= componentPool->sparseCapacity)
    return false;

  uint32_t index = componentPool->sparse[handle];

  return index != UINT32_MAX && index < componentPool->count;
}

uint32_t ComponentCreate(componentPool_t *pool) {

  uint32_t handle;

  // ---- reuse freed handles ----
  if (pool->freeCount > 0) {
    handle = pool->freeHandles[--pool->freeCount];
  } else {
    handle = pool->nextHandle++;
  }

  ComponentPoolEnsureSparse(pool, handle);

  if (pool->count >= pool->denseCapacity)
    ComponentPoolGrow(pool);

  uint32_t index = pool->count++;

  pool->denseHandles[index] = handle;
  pool->sparse[handle] = index;

  void *data = (char *)pool->denseData + index * pool->elementSize;

  memset(data, 0, pool->elementSize);

  return handle;
}

void ComponentPoolClear(componentPool_t *pool) {

  pool->count = 0;
  pool->nextHandle = 0;
  pool->freeCount = 0;

  if (pool->sparse) {
    for (uint32_t i = 0; i < pool->sparseCapacity; ++i)
      pool->sparse[i] = UINT32_MAX;
  }
}
