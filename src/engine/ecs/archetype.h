
#pragma once
#include "../util/bitset.h"
#include "archetype_internal.h"
#include "component.h"
#include "ecs_types.h"
#include <stdint.h>

typedef struct archetype_t archetype_t;

void ArchetypeInit(archetype_t *archetype, bitset_t mask);
void ArchetypeShutdown(archetype_t *archetype);

uint32_t ArchetypeAddEntity(archetype_t *archetype, entity_t entity);

archetypeColumn_t *ArchetypeFindColumn(archetype_t *arch,
                                       componentId_t componentId);

void ArchetypeAddInline(archetype_t *arch, componentId_t id, size_t size);

void ArchetypeAddHandle(archetype_t *arch, componentId_t id,
                        componentPool_t *pool);

static inline bool ArchetypeHas(archetype_t *arch, uint32_t comp) {
  return BitsetTest(&arch->mask, comp);
}
