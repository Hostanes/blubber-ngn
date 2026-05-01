#include "game.h"
#include "rlgl.h"

Engine EngineInit(void) {
  SetConfigFlags(FLAG_VSYNC_HINT);
  InitWindow(1280, 720, "ECS FPS Test");
  DisableCursor();
  SetExitKey(KEY_NULL);
  SetTargetFPS(60);

  Engine engine = {0};
  rlSetClipPlanes(0.1f, 5000.0f);
  engine.camera.fovy = 75.0f;
  engine.camera.projection = CAMERA_PERSPECTIVE;

  engine.world = WorldCreate();

  ComponentPoolInit(&engine.timerPool, sizeof(Timer));
  ComponentPoolInit(&engine.modelPool, sizeof(ModelCollection_t));

  return engine;
}

void EngineShutdown(Engine *engine) {
  (void)engine;
  CloseWindow();
}
