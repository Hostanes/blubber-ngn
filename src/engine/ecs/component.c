#include "component.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void ComponentPoolInit(componentPool_t *componentPool, size_t elementSize) {
  componentPool->denseEntities = NULL;
  componentPool->denseData = NULL;
  componentPool->sparse = NULL;

  componentPool->count = 0;
  componentPool->denseCapacity = 0;
  componentPool->sparseCapacity = 0;
  componentPool->elementSize = elementSize;
}

static void ComponentPoolGrow(componentPool_t *componentPool) {
  uint32_t oldCapacity = componentPool->denseCapacity;
  uint32_t newCapacity = oldCapacity == 0 ? 64 : oldCapacity * 2;

  componentPool->denseEntities =
      realloc(componentPool->denseEntities, newCapacity * sizeof(entity_t));

  componentPool->denseData = realloc(componentPool->denseData,
                                     newCapacity * componentPool->elementSize);

  componentPool->denseCapacity = newCapacity;
}

static void ComponentPoolEnsureSparse(componentPool_t *pool,
                                      uint32_t entityId) {
  uint32_t required = entityId + 1;

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

void *ComponentAdd(componentPool_t *componentPool, entity_t entity) {
  ComponentPoolEnsureSparse(componentPool, entity.id);

  if (ComponentHas(componentPool, entity)) {
    return ComponentGet(componentPool, entity);
  }

  if (componentPool->count >= componentPool->denseCapacity) {
    ComponentPoolGrow(componentPool);
  }

  uint32_t index = componentPool->count++;

  componentPool->denseEntities[index] = entity;
  componentPool->sparse[entity.id] = index;

  void *data =
      (char *)componentPool->denseData + index * componentPool->elementSize;

  memset(data, 0, componentPool->elementSize);

  return data;
}

// TODO test the performance of this method
// removing should hopefully happen rarely, maybe only during loading times
void ComponentRemove(componentPool_t *componentPool, entity_t entity) {

  if (!ComponentHas(componentPool, entity)) {
    return;
  }

  uint32_t index = componentPool->sparse[entity.id];
  uint32_t last = componentPool->count - 1;

  if (index != last) {
    // move last element into removed slot
    componentPool->denseEntities[index] = componentPool->denseEntities[last];

    memcpy((char *)componentPool->denseData +
               index * componentPool->elementSize,
           (char *)componentPool->denseData + last * componentPool->elementSize,
           componentPool->elementSize);

    // update sparse index for moved entity
    entity_t moved = componentPool->denseEntities[index];
    componentPool->sparse[moved.id] = index;
  }

  // unset in sparse array
  componentPool->sparse[entity.id] = UINT32_MAX;
  componentPool->count--;
}

void *ComponentGet(componentPool_t *componentPool, entity_t entity) {
  if (!ComponentHas(componentPool, entity)) {
    return NULL;
  }

  uint32_t index = componentPool->sparse[entity.id];
  return (char *)componentPool->denseData + index * componentPool->elementSize;
}

bool ComponentHas(const componentPool_t *componentPool, entity_t entity) {
  if (!componentPool->sparse || entity.id >= componentPool->sparseCapacity) {
    return false;
  }

  uint32_t id = entity.id;
  uint32_t index = componentPool->sparse[id];

  // check if entity assigned this comp && if this index still active
  return index != UINT32_MAX && index < componentPool->count;
}
