#include "world.h"
#include "archetype.h"
#include "world_internal.h"
#include <stdlib.h>
#include <string.h>

static archetype_t *WorldFindArchetype(world_t *world, const bitset_t *mask) {
  for (uint32_t i = 0; i < world->archetypeCount; ++i) {
    if (BitsetEquals(&world->archetypes[i].mask, mask)) {
      return &world->archetypes[i];
    }
  }
  return NULL;
}

archetype_t *WorldCreateArchetype(world_t *world, const bitset_t *mask) {
  if (world->archetypeCount >= world->archetypeCapacity) {
    uint32_t oldCap = world->archetypeCapacity;
    uint32_t newCap = oldCap == 0 ? 8 : oldCap * 2;

    world->archetypes =
        realloc(world->archetypes, newCap * sizeof(archetype_t));

    memset(world->archetypes + oldCap, 0,
           (newCap - oldCap) * sizeof(archetype_t));

    world->archetypeCapacity = newCap;
  }

  archetype_t *arch = &world->archetypes[world->archetypeCount++];
  ArchetypeInit(arch, *mask);

  return arch;
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

  archetype_t *arch = WorldFindArchetype(world, mask);
  if (!arch) {
    arch = WorldCreateArchetype(world, mask);
  }

  uint32_t index = ArchetypeAddEntity(arch, entity);

  world->entityLocations[entity.id].archetype = arch;
  world->entityLocations[entity.id].index = index;

  return entity;
}

void WorldDestroyEntity(world_t *world, entity_t entity) {
  if (!EntityIsAlive(&world->entityManager, entity)) {
    return;
  }

  /* removal not implemented yet */

  EntityDestroy(&world->entityManager, entity);
}

void *WorldGetComponent(world_t *world, entity_t entity,
                        componentId_t componentId) {
  if (!EntityIsAlive(&world->entityManager, entity)) {
    return NULL;
  }

  entityLocation_t loc = world->entityLocations[entity.id];
  archetype_t *arch = loc.archetype;

  archetypeColumn_t *col = ArchetypeFindColumn(arch, componentId);

  if (!col)
    return NULL;

  if (col->storageType == ArchetypeStorageInline) {
    return (char *)col->data + loc.index * col->elementSize;
  } else {
    return ComponentGet(col->pool, entity);
  }
}
