#include "component_registry.h"
#include "archetype.h"
#include <stdlib.h>
#include <string.h>

void ComponentRegistry_Init(ComponentRegistry *reg) {
  reg->defs = NULL;
  reg->count = 0;
  reg->capacity = 0;
}

void ComponentRegistry_Shutdown(ComponentRegistry *reg) {
  free(reg->defs);
  reg->defs = NULL;
  reg->count = 0;
  reg->capacity = 0;
}

void ComponentRegistry_Add(ComponentRegistry *reg, const char *name,
                            componentId_t id, size_t size,
                            archetypeStorageType_t storage,
                            componentPool_t *pool) {
  if (reg->count >= reg->capacity) {
    uint32_t newCap = reg->capacity == 0 ? 32 : reg->capacity * 2;
    reg->defs = realloc(reg->defs, newCap * sizeof(ComponentDef));
    reg->capacity = newCap;
  }

  reg->defs[reg->count++] = (ComponentDef){
      .name = name,
      .id = id,
      .size = size,
      .storage = storage,
      .pool = pool,
  };
}

const ComponentDef *ComponentRegistry_Find(const ComponentRegistry *reg,
                                           const char *name) {
  for (uint32_t i = 0; i < reg->count; ++i) {
    if (strcmp(reg->defs[i].name, name) == 0)
      return &reg->defs[i];
  }
  return NULL;
}

bool ComponentRegistry_AddToArchetype(const ComponentRegistry *reg,
                                      archetype_t *arch, const char *name) {
  const ComponentDef *def = ComponentRegistry_Find(reg, name);
  if (!def)
    return false;

  if (def->size == 0)
    return true; // tag component: contributes to mask only, no column

  if (def->storage == ArchetypeStorageInline)
    ArchetypeAddInline(arch, def->id, def->size);
  else
    ArchetypeAddHandle(arch, def->id, def->pool);

  return true;
}
