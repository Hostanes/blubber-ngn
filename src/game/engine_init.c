
#include "game.h"

Engine EngineInit(void) {
  SetConfigFlags(FLAG_VSYNC_HINT);
  InitWindow(1280, 720, "Benchmark A - OMP");

  Engine engine = {0};
  engine.world = WorldCreate();
  ComponentPoolInit(&engine.timerPool, sizeof(Timer));
  return engine;
}

void EngineShutdown(Engine *engine) {
  (void)engine;
  CloseWindow();
}
