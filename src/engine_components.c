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

void getComponent(ActorComponents_t *actors, entity_t entity, int componentId) {
  //
  //
}

void RemoveComponentFromElement(ActorComponents_t *actors, entity_t entity) {
  //
  //
}
