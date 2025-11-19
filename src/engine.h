
#ifndef ENGINE_H
#define ENGINE_H
#include "game.h"
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
} EngineConfig;

typedef struct Engine {
  EngineConfig config;

  // ----------------------------------------
  // ECS moved from GameState_t
  // ----------------------------------------
  EntityManager_t em;
  ActorComponents_t actors;
  ProjectilePool_t projectiles;
  StaticPool_t statics;
  ParticlePool_t particles;

  Terrain_t terrain; // yes, this too belongs in engine eventually
  EntityGrid_t grid; // engine owns the world
} Engine;

// Initializes the engine with the given configuration.
// Currently this only stores config and initializes the window.
// Later: camera, ECS, pools, systems, etc.
void engine_init(struct Engine *eng, const EngineConfig *cfg);

// Shutdown logic (stub for now)
void engine_shutdown(void);

Engine *engine_get(void);

#endif
