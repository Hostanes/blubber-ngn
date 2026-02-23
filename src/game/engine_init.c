
#include "game.h"

Engine EngineInit(void) {
  SetConfigFlags(FLAG_VSYNC_HINT);
  InitWindow(1280, 720, "ECS FPS Test");
  DisableCursor();
  SetTargetFPS(60);

  Engine engine = {0};

  engine.camera.fovy = 75.0f;
  engine.camera.projection = CAMERA_PERSPECTIVE;

  engine.world = WorldCreate();

  ComponentPoolInitAligned(&engine.timerPool, sizeof(Timer));

  return engine;
}

void EngineShutdown(Engine *engine) {
  (void)engine;
  CloseWindow();
}
