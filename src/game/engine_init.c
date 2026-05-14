
#include "game.h"

Engine EngineInit(void) {
  SetConfigFlags(FLAG_VSYNC_HINT);
  InitWindow(1280, 720, "ECS Benchmark");

  Engine engine = {0};
  engine.world = WorldCreate();
  return engine;
}

void EngineShutdown(Engine *engine) {
  (void)engine;
  CloseWindow();
}
