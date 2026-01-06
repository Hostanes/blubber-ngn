#include "engine.h"
#include "engine_components.h"
#include "raylib.h"
#include <stdlib.h>
#include <string.h>

// Global engine pointer
static Engine_t *g_engine = NULL;

Engine_t *engine_get(void) { return g_engine; }

void engine_init(struct Engine *eng, const struct EngineConfig *cfg) {
  g_engine = eng;
  g_engine->config = *cfg;

  SetConfigFlags(
      FLAG_VSYNC_HINT); // must come BEFORE InitWindow if you use flags

  InitWindow(cfg->window_width, cfg->window_height, "Blubber NGN");

  eng->em.count = 0;
  memset(eng->em.alive, 0, sizeof(eng->em.alive));
  memset(eng->em.masks, 0, sizeof(eng->em.masks));

  eng->actors = malloc(sizeof(ActorComponents_t));
  memset(eng->actors, 0, sizeof(ActorComponents_t));
  memset(&eng->projectiles, 0, sizeof(eng->projectiles));
  memset(&eng->statics, 0, sizeof(eng->statics));
  memset(&eng->particles, 0, sizeof(eng->particles));

  // future: init camera, ecs, pools, etc
}

void engine_shutdown(void) {
  CloseWindow();
  g_engine = NULL;
}
