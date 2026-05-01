#pragma once
#include "archetype.h"
#include "archetype_internal.h"
#include "component.h"
#include "ecs_types.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  const char *name;
  componentId_t id;
  size_t size;                    // sizeof(ActualComponent)
  archetypeStorageType_t storage;
  componentPool_t *pool;          // NULL for inline
} ComponentDef;

typedef struct {
  ComponentDef *defs;
  uint32_t count;
  uint32_t capacity;
} ComponentRegistry;

void ComponentRegistry_Init(ComponentRegistry *reg);
void ComponentRegistry_Shutdown(ComponentRegistry *reg);

void ComponentRegistry_Add(ComponentRegistry *reg, const char *name,
                            componentId_t id, size_t size,
                            archetypeStorageType_t storage,
                            componentPool_t *pool);

const ComponentDef *ComponentRegistry_Find(const ComponentRegistry *reg,
                                           const char *name);

bool ComponentRegistry_AddToArchetype(const ComponentRegistry *reg,
                                      archetype_t *arch, const char *name);
