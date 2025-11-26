// main.c
// Entry point for the refactored Mech Arena demo.
// Depends on: game.h, sound.h, systems.h, raylib, raymath

#include "game.h"

#include "engine.h"
#include "raylib.h"
#include "sound.h"
#include "systems/systems.h"
#include <raymath.h>
#include <stdio.h>
#include <sys/types.h>

int main(void) {
  printf("raylib version: %s\n", RAYLIB_VERSION);

  SetConfigFlags(FLAG_VSYNC_HINT);

  // --------------------------------------------
  // ENGINE CONFIG
  // --------------------------------------------
  EngineConfig_t cfg = {
      .window_width = 1280,
      .window_height = 720,

      .fov_deg = 60.0f,
      .near_plane = 0.1f,
      .far_plane = 5000.0f,

      .max_entities = 2048,
      .max_projectiles = 256,
      .max_actors = 256,
      .max_particles = 4096,
      .max_statics = 1024,
  };

  Engine_t eng;
  engine_init(&eng, &cfg);

  EnableCursor();
  SetTargetFPS(60);

  LoadAssets();

  // --------------------------------------------
  // CAMERA SETUP (still here for now)
  // --------------------------------------------
  Camera3D camera = {0};
  camera.position = (Vector3){0, 0, 0};
  camera.target = (Vector3){0, 0, 0};
  camera.up = (Vector3){0, 1, 0};
  camera.fovy = cfg.fov_deg;
  camera.projection = CAMERA_PERSPECTIVE;

  SetMasterVolume(0.1f);

  GameState_t gs = InitGame(engine_get());
  SoundSystem_t soundSys = InitSoundSystem();

  gs.state = STATE_MAINMENU;

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    UpdateGame(&gs, &eng, &soundSys, &camera, dt);
  }

  CloseAudioDevice();
  engine_shutdown();

  return 0;
}
