#include "archetype.h"
#include "archetype_internal.h"
#include "component.h"
#include <stdlib.h>
#include <string.h>

static void ArchetypeGrow(archetype_t *archetype) {
  uint32_t oldCap = archetype->capacity;
  uint32_t newCap = oldCap == 0 ? 64 : oldCap * 2;

  archetype->entities = realloc(archetype->entities, newCap * sizeof(entity_t));

  for (uint32_t i = 0; i < archetype->columnCount; ++i) {
    archetypeColumn_t *col = &archetype->columns[i];

    if (col->storageType == ArchetypeStorageInline) {
      col->data = realloc(col->data, newCap * col->elementSize);
    }
  }

  archetype->capacity = newCap;
}

void ArchetypeInit(archetype_t *archetype, bitset_t mask) {
  archetype->mask = mask;

  archetype->entities = NULL;
  archetype->count = 0;
  archetype->capacity = 0;

  archetype->columns = NULL;
  archetype->columnCount = 0;
}

void ArchetypeShutdown(archetype_t *archetype) {
  for (uint32_t i = 0; i < archetype->columnCount; ++i) {
    archetypeColumn_t *col = &archetype->columns[i];

    if (col->storageType == ArchetypeStorageInline) {
      free(col->data);
    }
  }

  free(archetype->columns);
  free(archetype->entities);

  archetype->columns = NULL;
  archetype->entities = NULL;
  archetype->count = 0;
  archetype->capacity = 0;
}

uint32_t ArchetypeAddEntity(archetype_t *archetype, entity_t entity) {
  if (archetype->count >= archetype->capacity) {
    ArchetypeGrow(archetype);
  }

  uint32_t index = archetype->count++;

  archetype->entities[index] = entity;

  for (uint32_t i = 0; i < archetype->columnCount; ++i) {
    archetypeColumn_t *col = &archetype->columns[i];

    if (col->storageType == ArchetypeStorageInline) {
      void *dst = (char *)col->data + index * col->elementSize;
      memset(dst, 0, col->elementSize);
    } else {
      ComponentAdd(col->pool, entity);
    }
  }

  return index;
}

static archetypeColumn_t *ArchetypeAddColumn(archetype_t *archetype,
                                             componentId_t componentId) {

  archetype->columns =
      realloc(archetype->columns,
              (archetype->columnCount + 1) * sizeof(archetypeColumn_t));

  archetypeColumn_t *col = &archetype->columns[archetype->columnCount++];

  memset(col, 0, sizeof(*col));
  col->componentId = componentId;

  return col;
}

void ArchetypeAddInlineColumn(archetype_t *archetype, componentId_t componentId,
                              size_t elementSize) {

  archetypeColumn_t *col = ArchetypeAddColumn(archetype, componentId);

  col->storageType = ArchetypeStorageInline;
  col->elementSize = elementSize;
  col->data = NULL;
}

void ArchetypeAddPoolColumn(archetype_t *archetype, componentId_t componentId,
                            componentPool_t *pool) {

  archetypeColumn_t *col = ArchetypeAddColumn(archetype, componentId);

  col->storageType = ArchetypeStoragePool;
  col->pool = pool;
}

archetypeColumn_t *ArchetypeFindColumn(archetype_t *arch,
                                       componentId_t componentId) {
  for (uint32_t i = 0; i < arch->columnCount; ++i) {
    if (arch->columns[i].componentId == componentId) {
      return &arch->columns[i];
    }
  }
  return NULL;
}

void ArchetypeAddInline(archetype_t *arch, componentId_t id, size_t size) {
  arch->columns = realloc(arch->columns,
                          (arch->columnCount + 1) * sizeof(archetypeColumn_t));

  archetypeColumn_t *col = &arch->columns[arch->columnCount++];

  col->componentId = id;
  col->storageType = ArchetypeStorageInline;
  col->elementSize = size;
  col->data = NULL;
  col->pool = NULL;
}

void ArchetypeAddPool(archetype_t *arch, componentId_t id,
                      componentPool_t *pool) {
  arch->columns = realloc(arch->columns,
                          (arch->columnCount + 1) * sizeof(archetypeColumn_t));

  archetypeColumn_t *col = &arch->columns[arch->columnCount++];

  col->componentId = id;
  col->storageType = ArchetypeStoragePool;
  col->elementSize = 0;
  col->data = NULL;
  col->pool = pool;
}
