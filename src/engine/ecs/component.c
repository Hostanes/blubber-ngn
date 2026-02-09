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

  if (required <= pool->sparseCapacity) {
    return;
  }

  uint32_t oldCap = pool->sparseCapacity;
  uint32_t newCap = oldCap == 0 ? 64 : oldCap;

  while (newCap < required) {
    newCap *= 2;
  }

  pool->sparse = realloc(pool->sparse, newCap * sizeof(uint32_t));

  for (uint32_t i = oldCap; i < newCap; ++i) {
    pool->sparse[i] = UINT32_MAX;
  }

  pool->sparseCapacity = newCap;
}

// componentId_t ComponentRegister(size_t elementSize, const char *name);

// TODO test the performance of this method
// removing should hopefully happen rarely, maybe only during loading times
void ComponentRemove(componentPool_t *componentPool, uint32_t handle) {

  if (!ComponentHas(componentPool, handle)) {
    return;
  }

  uint32_t index = componentPool->sparse[handle];
  uint32_t last = componentPool->count - 1;

  if (index != last) {
    // move last element into removed slot
    componentPool->denseHandles[index] = componentPool->denseHandles[last];

    memcpy((char *)componentPool->denseData +
               index * componentPool->elementSize,
           (char *)componentPool->denseData + last * componentPool->elementSize,
           componentPool->elementSize);

    // update sparse index for moved entity
    uint32_t moved = componentPool->denseHandles[index];
    componentPool->sparse[moved] = index;
  }

  // unset in sparse array
  componentPool->sparse[handle] = UINT32_MAX;
  componentPool->count--;
}

void *ComponentGet(componentPool_t *componentPool, uint32_t handle) {
  if (!ComponentHas(componentPool, handle)) {
    return NULL;
  }

  uint32_t index = componentPool->sparse[handle];
  return (char *)componentPool->denseData + index * componentPool->elementSize;
}

bool ComponentHas(const componentPool_t *componentPool, uint32_t handle) {
  if (!componentPool->sparse || handle >= componentPool->sparseCapacity) {
    return false;
  }

  uint32_t id = handle;
  uint32_t index = componentPool->sparse[id];

  // check if entity assigned this comp && if this index still active
  return index != UINT32_MAX && index < componentPool->count;
}

// add functiont hat creates its own handle
// as opposed to taking one through parameters
uint32_t ComponentCreate(componentPool_t *pool) {
  uint32_t handle = pool->nextHandle++;
  ComponentPoolEnsureSparse(pool, handle);

  if (pool->count >= pool->denseCapacity) {
    ComponentPoolGrow(pool);
  }

  uint32_t index = pool->count++;

  pool->denseHandles[index] = handle;
  pool->sparse[handle] = index;

  void *data = (char *)pool->denseData + index * pool->elementSize;

  memset(data, 0, pool->elementSize);

  return handle;
}
