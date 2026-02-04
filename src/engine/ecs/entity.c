
#include "entity.h"
#include "entity_internal.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

void EntityManagerInit(entityManager_t *entityManager) {
  entityManager->generations = NULL;
  entityManager->freeIds = NULL;
  entityManager->freeCount = 0;
  entityManager->capacity = 0;
  entityManager->nextId = 0;
}

void EntityManagerShutdown(entityManager_t *entityManager) {
  free(entityManager->generations);
  free(entityManager->freeIds);

  entityManager->generations = NULL;
  entityManager->freeIds = NULL;
  entityManager->freeCount = 0;
  entityManager->capacity = 0;
  entityManager->nextId = 0;
}

// dynamic array style realloc to increase max nb of entities
// start value is 1024, TODO change start entity capacity to config nb
static void EntityManagerGrow(entityManager_t *entityManager) {
  uint32_t oldCapacity = entityManager->capacity;
  uint32_t newCapacity = oldCapacity == 0 ? 1024 : oldCapacity * 2;

  entityManager->generations =
      realloc(entityManager->generations, newCapacity * sizeof(uint32_t));

  // Initialize new generations to 0
  memset(entityManager->generations + oldCapacity, 0,
         (newCapacity - oldCapacity) * sizeof(uint32_t));

  entityManager->capacity = newCapacity;
}

// regrow entitymanager if not enough space
entity_t EntityCreate(entityManager_t *entityManager) {
  uint32_t id;

  if (entityManager->freeCount > 0) {
    id = entityManager->freeIds[--entityManager->freeCount];
  } else {
    if (entityManager->capacity == 0 ||
        entityManager->capacity <= entityManager->nextId) {
      EntityManagerGrow(entityManager);
    }

    id = entityManager->nextId++;
  }

  entity_t entity;
  entity.id = id;
  entity.generation = entityManager->generations[id];
  entity.type = 0; // TODO fix, default type for now

  return entity;
}

void EntityDestroy(entityManager_t *entityManager, entity_t entity) {
  if (!EntityIsAlive(entityManager, entity)) {
    return;
  }

  uint32_t id = entity.id;

  // only increment generations on destroys
  entityManager->generations[id]++;

  entityManager->freeIds =
      realloc(entityManager->freeIds,
              (entityManager->freeCount + 1) * sizeof(uint32_t));

  entityManager->freeIds[entityManager->freeCount++] = id;
}

// check if entity exists in bounds, and if generations nbs match up
bool EntityIsAlive(const entityManager_t *entityManager, entity_t entity) {
  if (entity.id >= entityManager->capacity) {
    return false;
  }

  return entityManager->generations[entity.id] == entity.generation;
}
