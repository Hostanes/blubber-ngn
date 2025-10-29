
// main.c
// Entry point for the refactored Mech Arena demo.
// Depends on: game.h, sound.h, systems.h, raylib, raymath

#include <math.h>
#include <stdio.h>

#include "raylib.h"
#include "raymath.h"

#include "game.h"
#include "sound.h"
#include "systems.h"

int main(void) {
  printf("raylib version: %s\n", RAYLIB_VERSION);

  int screenWidth = 1280;
  int screenHeight = 720;

  InitWindow(screenWidth, screenHeight, "Mech Arena Demo (refactor)");

  SetConfigFlags(FLAG_VSYNC_HINT);
  InitWindow(screenWidth, screenHeight, "MechArenaDemo");

  EnableCursor();

  SetTargetFPS(60);

  LoadAssets();

  Camera3D camera = {0};
  camera.position = (Vector3){0, 0, 0};
  camera.target = (Vector3){0, 0, 0};
  camera.up = (Vector3){0, 1, 0};
  camera.fovy = 60.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  SetMasterVolume(1.0f);

  GameState_t gs = InitGame();
  SoundSystem_t soundSys = InitSoundSystem();

  gs.state = STATE_MAINMENU;

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    UpdateGame(&gs, &soundSys, &camera, dt);
  }

  CloseAudioDevice();
  CloseWindow();

  return 0;
}
