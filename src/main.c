
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

  DisableCursor(); // lock mouse for torso look
  SetTargetFPS(60);

  LoadAssets();

  Camera3D camera = {0};
  camera.position = (Vector3){0, 10, -10};
  camera.target = (Vector3){0, 1, 0};
  camera.up = (Vector3){0, 1, 0};
  camera.fovy = 60.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  SetMasterVolume(1.0f);

  GameState_t gs = InitGame();
  SoundSystem_t soundSys = InitSoundSystem();

  const float bobAmount = 0.5f; // height in meters, visual only

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    // Systems
    PlayerControlSystem(&gs, &soundSys, dt);
    PhysicsSystem(&gs, dt);

    // Camera setup based on torso orientation
    int pid = gs.playerId;
    Vector3 playerPos = gs.entities.positions[pid];
    Orientation torso = gs.entities.modelCollections[0].orientations[1];

    Vector3 forward = {cosf(torso.pitch) * cosf(torso.yaw), sinf(torso.pitch),
                       cosf(torso.pitch) * sinf(torso.yaw)};

    // Headbob / eye offset computed from step cycle
    float t = gs.entities.stepCycle[pid];
    float bobTri = (t < 0.5f) ? (t * 2.0f) : (2.0f - t * 2.0f); // 0 -> 1 -> 0
    bobTri = 1.0f - bobTri; // flip to make "drop"
    float bobY = bobTri * bobAmount;

    Vector3 eye =
        (Vector3){playerPos.x, playerPos.y + 10.0f + bobY, playerPos.z};
    camera.position = eye;
    camera.target = Vector3Add(eye, forward);

    // Render & audio processing
    RenderSystem(&gs, camera);
    ProcessSoundSystem(&soundSys, playerPos);
  }

  CloseAudioDevice();
  CloseWindow();

  return 0;
}
