#include <string.h>
#include "engine.h"
#include "raylib.h"
#include <stdlib.h>

// Global engine pointer
static Engine *g_engine = NULL;

Engine *engine_get(void) { return g_engine; }

void engine_init(Engine *eng, const EngineConfig *cfg) {
  g_engine = eng;          // <-- Store the pointer first (critical)
  g_engine->config = *cfg; // now safe

  SetConfigFlags(
      FLAG_VSYNC_HINT); // must come BEFORE InitWindow if you use flags

  InitWindow(cfg->window_width, cfg->window_height, "Blubber NGN");

  eng->em.count = 0;
  memset(eng->em.alive, 0, sizeof(eng->em.alive));
  memset(eng->em.masks, 0, sizeof(eng->em.masks));

  memset(&eng->actors, 0, sizeof(eng->actors));
  memset(&eng->projectiles, 0, sizeof(eng->projectiles));
  memset(&eng->statics, 0, sizeof(eng->statics));
  memset(&eng->particles, 0, sizeof(eng->particles));

  // future: init camera, ecs, pools, etc
}

void engine_shutdown(void) {
  CloseWindow();
  g_engine = NULL;
}
