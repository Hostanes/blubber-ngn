
#ifndef ENGINE_H
#define ENGINE_H

#include "engine_components.h"
#include <stdbool.h>
#include <stdint.h>

typedef struct EngineConfig {
  int window_width;
  int window_height;

  float fov_deg;
  float near_plane;
  float far_plane;

  int max_entities;
  int max_projectiles;
  int max_actors;
  int max_particles;
  int max_statics;
} EngineConfig_t;

typedef struct Engine {
  EngineConfig_t config;

  EntityManager_t em;
  ActorComponents_t actors;
  ProjectilePool_t projectiles;
  StaticPool_t statics;
  ParticlePool_t particles;

} Engine_t;

// Initializes the engine with the given configuration.
// Currently this only stores config and initializes the window.
// Later: camera, ECS, pools, systems, etc.
void engine_init(struct Engine *eng, const struct EngineConfig *cfg);

// Shutdown logic (stub for now)
void engine_shutdown(void);

Engine_t *engine_get(void);

//
//  Engine Component Store
//

int registerComponent(ActorComponents_t *actors, size_t elementSize);

void addComponentToElement(EntityManager_t *em, ActorComponents_t *actors,
                           entity_t entity, int componentId,
                           void *elementValue);

void *getComponent(ActorComponents_t *actors, entity_t entity, int componentId);

void RemoveComponentFromElement(ActorComponents_t *actors, entity_t entity);

#endif
