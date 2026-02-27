#include "world.h"
#include "archetype.h"
#include "world_internal.h"
#include <stdlib.h>
#include <string.h>

static int32_t WorldFindArchetype(world_t *world, const bitset_t *mask) {
  for (uint32_t i = 0; i < world->archetypeCount; ++i) {
    if (BitsetEquals(&world->archetypes[i].mask, mask)) {
      return (int32_t)i;
    }
  }
  return -1;
}

uint32_t WorldCreateArchetype(world_t *world, const bitset_t *mask) {
  if (world->archetypeCount >= world->archetypeCapacity) {
    uint32_t oldCap = world->archetypeCapacity;
    uint32_t newCap = oldCap == 0 ? 8 : oldCap * 2;

    world->archetypes =
        realloc(world->archetypes, newCap * sizeof(archetype_t));

    memset(world->archetypes + oldCap, 0,
           (newCap - oldCap) * sizeof(archetype_t));

    world->archetypeCapacity = newCap;
  }

  uint32_t index = world->archetypeCount++;
  archetype_t *arch = &world->archetypes[index];

  ArchetypeInit(arch, *mask);

  return index;
}

world_t *WorldCreate(void) {
  world_t *world = calloc(1, sizeof(world_t));
  EntityManagerInit(&world->entityManager);
  return world;
}

void WorldDestroy(world_t *world) {
  for (uint32_t i = 0; i < world->archetypeCount; ++i) {
    ArchetypeShutdown(&world->archetypes[i]);
  }

  free(world->entityLocations);
  EntityManagerShutdown(&world->entityManager);
  free(world);
}

entity_t WorldCreateEntity(world_t *world, const bitset_t *mask) {
  entity_t entity = EntityCreate(&world->entityManager);

  if (entity.id >= world->entityLocationCapacity) {
    uint32_t oldCap = world->entityLocationCapacity;
    uint32_t newCap = oldCap == 0 ? 1024 : oldCap * 2;

    while (newCap <= entity.id) {
      newCap *= 2;
    }

    world->entityLocations =
        realloc(world->entityLocations, newCap * sizeof(entityLocation_t));

    memset(world->entityLocations + oldCap, 0,
           (newCap - oldCap) * sizeof(entityLocation_t));

    world->entityLocationCapacity = newCap;
  }

  int32_t archIndex = WorldFindArchetype(world, mask);
  if (archIndex < 0) {
    archIndex = WorldCreateArchetype(world, mask);
  }

  archetype_t *arch = &world->archetypes[archIndex];

  uint32_t index = ArchetypeAddEntity(arch, entity);

  world->entityLocations[entity.id].archetype = archIndex;
  world->entityLocations[entity.id].index = index;

  return entity;
}

void WorldDestroyEntity(world_t *world, entity_t entity) {

  if (!EntityIsAlive(&world->entityManager, entity)) {
    return;
  }

  // Get entity location
  entityLocation_t loc = world->entityLocations[entity.id];
  archetype_t *arch = &world->archetypes[loc.archetype];

  // Store the last entity in this archetype BEFORE removal
  entity_t lastEntity = (loc.index < arch->count - 1)
                            ? arch->entities[arch->count - 1]
                            : INVALID_ENTITY;

  // Remove entity from archetype (this handles component cleanup)
  ArchetypeRemoveEntity(arch, loc.index);

  // If we moved an entity, update its location
  if (lastEntity.id != UINT32_MAX && loc.index != arch->count) {
    world->entityLocations[lastEntity.id].index = loc.index;
  }

  // Clear this entity's location
  world->entityLocations[entity.id].archetype = UINT32_MAX;
  world->entityLocations[entity.id].index = UINT32_MAX;

  // Finally, destroy the entity in the entity manager
  EntityDestroy(&world->entityManager, entity);
}

void *WorldGetComponent(world_t *world, entity_t entity,
                        componentId_t componentId) {
  if (!EntityIsAlive(&world->entityManager, entity)) {
    return NULL;
  }

  entityLocation_t loc = world->entityLocations[entity.id];
  archetype_t *arch = &world->archetypes[loc.archetype];

  archetypeColumn_t *col = ArchetypeFindColumn(arch, componentId);
  if (!col)
    return NULL;

  /* ---------- inline storage ---------- */
  if (col->storageType == ArchetypeStorageInline) {
    return (char *)col->data + loc.index * col->elementSize;
  }

  /* ---------- handle storage ---------- */
  uint32_t *handleArray = (uint32_t *)col->data;
  uint32_t handle = handleArray[loc.index];

  return ComponentGet((componentPool_t *)col->pool, handle);
}

void WorldClear(world_t *world) {
  for (uint32_t i = 0; i < world->archetypeCount; ++i) {
    ArchetypeClear(&world->archetypes[i]);
  }

  EntityManagerClear(&world->entityManager);

  if (world->entityLocations) {
    // Using UINT32_MAX or 0 depends on your preference,
    // but ensuring archetype index is invalid is key.
    memset(world->entityLocations, 0xFF,
           world->entityLocationCapacity * sizeof(entityLocation_t));
  }
}
