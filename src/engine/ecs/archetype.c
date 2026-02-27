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

void ArchetypeRemoveEntity(archetype_t *arch, uint32_t index) {
  if (index >= arch->count)
    return;

  uint32_t lastIndex = arch->count - 1;
  entity_t lastEntity = arch->entities[lastIndex];

  for (uint32_t i = 0; i < arch->columnCount; ++i) {
    archetypeColumn_t *col = &arch->columns[i];
    void *data = (char *)col->data + index * col->elementSize;

    if (col->storageType == ArchetypeStorageHandle) {
      uint32_t handle = *(uint32_t *)data;
      if (handle != UINT32_MAX) {
        ComponentRemove(col->pool, handle);
      }
    }
  }

  if (index != lastIndex) {
    arch->entities[index] = lastEntity;

    for (uint32_t i = 0; i < arch->columnCount; ++i) {
      archetypeColumn_t *col = &arch->columns[i];
      void *src = (char *)col->data + lastIndex * col->elementSize;
      void *dst = (char *)col->data + index * col->elementSize;

      memcpy(dst, src, col->elementSize);
    }
  }

  arch->count--;
}

void ArchetypeClear(archetype_t *arch) {
  // Release components back to pools if they are Handle-based
  for (uint32_t i = 0; i < arch->columnCount; ++i) {
    archetypeColumn_t *col = &arch->columns[i];
    if (col->storageType == ArchetypeStorageHandle && col->pool) {
      uint32_t *handles = (uint32_t *)col->data;
      for (uint32_t j = 0; j < arch->count; ++j) {
        if (handles[j] != UINT32_MAX) {
          ComponentRemove(col->pool, handles[j]);
        }
      }
    }
  }
  arch->count = 0;
}
