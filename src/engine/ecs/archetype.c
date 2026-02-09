#include "archetype.h"
#include "archetype_internal.h"
#include "component.h"
#include <stdlib.h>
#include <string.h>

static void ArchetypeGrow(archetype_t *arch) {
  uint32_t oldCap = arch->capacity;
  uint32_t newCap = oldCap == 0 ? 64 : oldCap * 2;

  arch->entities = realloc(arch->entities, newCap * sizeof(entity_t));

  for (uint32_t i = 0; i < arch->columnCount; ++i) {
    archetypeColumn_t *col = &arch->columns[i];
    if (col->elementSize > 0) {
      if (!col->data) {
        col->data = calloc(newCap, col->elementSize);
      } else {
        col->data = realloc(col->data, newCap * col->elementSize);
      }
    }
  }

  arch->capacity = newCap;
}

void ArchetypeInit(archetype_t *arch, bitset_t mask) {
  arch->mask = mask;
  arch->entities = NULL;
  arch->count = 0;
  arch->capacity = 0;
  arch->columns = NULL;
  arch->columnCount = 0;
}

void ArchetypeShutdown(archetype_t *arch) {
  for (uint32_t i = 0; i < arch->columnCount; ++i) {
    free(arch->columns[i].data);
  }

  free(arch->columns);
  free(arch->entities);

  memset(arch, 0, sizeof(*arch));
}

static archetypeColumn_t *ArchetypeAddColumn(archetype_t *arch,
                                             componentId_t componentId,
                                             archetypeStorageType_t storageType,
                                             size_t elementSize) {

  arch->columns = realloc(arch->columns,
                          (arch->columnCount + 1) * sizeof(archetypeColumn_t));

  archetypeColumn_t *col = &arch->columns[arch->columnCount++];
  memset(col, 0, sizeof(*col));

  col->componentId = componentId;
  col->storageType = storageType;
  col->elementSize = elementSize;
  col->data = NULL;
  col->externalStore = NULL;

  return col;
}

void ArchetypeAddInline(archetype_t *arch, componentId_t id, size_t size) {
  ArchetypeAddColumn(arch, id, ArchetypeStorageInline, size);
}

void ArchetypeAddHandle(archetype_t *arch, componentId_t id,
                        componentPool_t *pool) {
  archetypeColumn_t *col =
      ArchetypeAddColumn(arch, id, ArchetypeStorageHandle, sizeof(uint32_t));
  col->pool = pool;
}

uint32_t ArchetypeAddEntity(archetype_t *arch, entity_t entity) {
  if (arch->count >= arch->capacity) {
    ArchetypeGrow(arch);
  }

  uint32_t index = arch->count++;
  arch->entities[index] = entity;

  for (uint32_t i = 0; i < arch->columnCount; ++i) {
    archetypeColumn_t *col = &arch->columns[i];
    void *dst = (char *)col->data + index * col->elementSize;
    if (col->storageType == ArchetypeStorageInline) {
      memset(dst, 0, col->elementSize);
    } else {
      uint32_t handle = ComponentCreate(col->pool);
      *(uint32_t *)dst = handle;
    }
  }

  return index;
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
