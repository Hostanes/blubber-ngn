#include "engine_components.h"
#include "engine.h"
#include <stdint.h>
#include <string.h>
#include <sys/types.h>

int registerComponent(ActorComponents_t *actors, size_t elementSize) {
  int componentId = actors->componentCount;

  ComponentStorage_t *cs = &actors->componentStore[componentId];
  cs->id = componentId;
  cs->elementSize = elementSize;
  cs->data = calloc(MAX_ENTITIES, elementSize);
  cs->occupied = calloc(MAX_ENTITIES, sizeof(bool));
  cs->count = 0;

  actors->componentCount++;
  return componentId;
}

void addComponentToElement(EntityManager_t *em, ActorComponents_t *actors,
                           entity_t entity, int componentId,
                           void *elementValue) {
  ComponentStorage_t *cs = &actors->componentStore[componentId];

  uint8_t *addr = (uint8_t *)cs->data + (entity * cs->elementSize);

  memcpy(addr, elementValue, cs->elementSize);

  cs->count++;
  cs->occupied[entity] = true;

  em->masks[entity] |= (1 << componentId);
}

void *getComponent(ActorComponents_t *actors, entity_t entity,
                   int componentId) {
  ComponentStorage_t *cs = &actors->componentStore[componentId];
  if (!cs->occupied[entity]) {
    return NULL;
  }

  return (uint8_t *)cs->data + (entity * cs->elementSize);
}

void removeComponentFromEntity(EntityManager_t *em, ActorComponents_t *actors,
                               entity_t entity, ComponentID id) {
  ComponentStorage_t *cs = &actors->componentStore[id];
  cs->occupied[entity] = false;

  // zero memory for safety
  memset((uint8_t *)cs->data + entity * cs->elementSize, 0, cs->elementSize);

  em->masks[entity] &= ~(1 << id);
}
