
#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include <math.h>

#define MAX_ENTITIES 1

// Stores data for all entities
typedef struct EntityData {
  int count;
  Vector3 *positions;
  Vector3 *velocities;
  float *legYaw;
  float *torsoYaw;
  float *torsoPitch;
} EntityData_t;

typedef struct GameState {
  EntityData_t entities;
  int playerId; // player isnt a unique struct, just entity[0]
} GameState_t;

// ==================== Init ====================

GameState_t InitGame(void) {
  GameState_t gs = {0};

  gs.entities.count = MAX_ENTITIES;

  gs.entities.positions = (Vector3 *)MemAlloc(sizeof(Vector3) * MAX_ENTITIES);
  gs.entities.velocities = (Vector3 *)MemAlloc(sizeof(Vector3) * MAX_ENTITIES);
  gs.entities.legYaw = (float *)MemAlloc(sizeof(float) * MAX_ENTITIES);
  gs.entities.torsoYaw = (float *)MemAlloc(sizeof(float) * MAX_ENTITIES);
  gs.entities.torsoPitch = (float *)MemAlloc(sizeof(float) * MAX_ENTITIES);

  // Init player
  gs.playerId = 0;
  gs.entities.positions[0] = (Vector3){0, 1, 0};
  gs.entities.velocities[0] = (Vector3){0, 0, 0};
  gs.entities.legYaw[0] = 0.0l;
  gs.entities.torsoYaw[0] = 0.0l;
  gs.entities.torsoPitch[0] = 0.0f;

  return gs;
}

// ==================== Systems ====================

void PlayerControlSystem(GameState_t *gs, float dt) {
  int pid = gs->playerId;
  Vector3 *pos = gs->entities.positions;
  Vector3 *vel = gs->entities.velocities;
  float *legYaw = gs->entities.legYaw;
  float *torsoYaw = gs->entities.torsoYaw;
  float *torsoPitch = gs->entities.torsoPitch;

  // rotate legs
  if (IsKeyDown(KEY_A))
    legYaw[pid] -= 1.5f * dt;
  if (IsKeyDown(KEY_D))
    legYaw[pid] += 1.5f * dt;

  // Torso yaw from mouse delta
  Vector2 mouse = GetMouseDelta();
  float mouseSensitivity = 0.001f;
  torsoYaw[pid] += mouse.x * mouseSensitivity;
  torsoPitch[pid] += -mouse.y * mouseSensitivity;

  // Clamp pitch between -89° and +89°
  if (torsoPitch[pid] > 1.55f)
    torsoPitch[pid] = 1.55f; // ~89°
  if (torsoPitch[pid] < -1.55f)
    torsoPitch[pid] = -1.55f; // ~-89°

  // Movement is still based on leg direction (not torso)
  float c = cosf(legYaw[pid]);
  float s = sinf(legYaw[pid]);
  Vector3 forward = {c, 0, s};
  Vector3 right = {-s, 0, c};

  // movement keys
  if (IsKeyDown(KEY_W)) {
    vel[pid].x += forward.x * 100.0f * dt; // forward fast
    vel[pid].z += forward.z * 100.0f * dt;
  }
  if (IsKeyDown(KEY_S)) {
    vel[pid].x -= forward.x * 40.0f * dt; // backward slower
    vel[pid].z -= forward.z * 40.0f * dt;
  }
  if (IsKeyDown(KEY_Q)) { // strafe left
    vel[pid].x += right.x * -40.0f * dt;
    vel[pid].z += right.z * -40.0f * dt;
  }
  if (IsKeyDown(KEY_E)) { // strafe right
    vel[pid].x += right.x * 40.0f * dt;
    vel[pid].z += right.z * 40.0f * dt;
  }
}

void PhysicsSystem(GameState_t *gs, float dt) {
  Vector3 *pos = gs->entities.positions;
  Vector3 *vel = gs->entities.velocities;

  for (int i = 0; i < gs->entities.count; i++) {
    pos[i].x += vel[i].x * dt;
    pos[i].y += vel[i].y * dt;
    pos[i].z += vel[i].z * dt;

    // simple drag
    vel[i].x *= 0.65f;
    vel[i].z *= 0.65f;
  }
}

void RenderSystem(GameState_t *gs, Camera3D camera) {
  BeginDrawing();
  ClearBackground((Color){20, 20, 30, 255});

  BeginMode3D(camera);

  // ground
  DrawPlane((Vector3){0, 0, 0}, (Vector2){50, 50}, GREEN);

  // walls
  for (int i = -25; i <= 25; i += 10) {
    DrawCube((Vector3){i, 1, -25}, 2, 2, 2, GRAY);
    DrawCube((Vector3){i, 1, 25}, 2, 2, 2, GRAY);
    DrawCube((Vector3){-25, 1, i}, 2, 2, 2, GRAY);
    DrawCube((Vector3){25, 1, i}, 2, 2, 2, GRAY);
  }

  // player arrow
  int pid = gs->playerId;
  Vector3 pos = gs->entities.positions[pid];
  float yaw = gs->entities.legYaw[pid];
  Vector3 forward = {cosf(yaw), 0, sinf(yaw)}; // 2d direction of legs

  Vector3 base = {pos.x, pos.y + 0.8f, pos.z};
  Vector3 shaftEnd = {base.x + forward.x * 0.95f, base.y,
                      base.z + forward.z * 0.95f};
  DrawCylinderEx(base, shaftEnd, 0.1f, 0.1f, 8, BLUE);

  Vector3 headEnd = {shaftEnd.x + forward.x * 0.55f, shaftEnd.y,
                     shaftEnd.z + forward.z * 0.55f};
  DrawCylinderEx(shaftEnd, headEnd, 0.25f, 0.0f, 8, RED);

  EndMode3D();

  DrawFPS(10, 10);
  EndDrawing();
}

// ========== Main ==========

int main(void) {
  InitWindow(1280, 720, "Mech Arena Demo (DoD style)");
  DisableCursor(); // lock mouse for torso look
  SetTargetFPS(60);

  Camera3D camera = {0};
  camera.position = (Vector3){0, 10, -10};
  camera.target = (Vector3){0, 1, 0};
  camera.up = (Vector3){0, 1, 0};
  camera.fovy = 60.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  GameState_t gs = InitGame();

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    PlayerControlSystem(&gs, dt);
    PhysicsSystem(&gs, dt);

    int pid = gs.playerId;
    Vector3 playerPos = gs.entities.positions[pid];
    float yaw = gs.entities.torsoYaw[pid];
    float pitch = gs.entities.torsoPitch[pid];

    Vector3 forward = {cosf(pitch) * cosf(yaw), sinf(pitch),
                       cosf(pitch) * sinf(yaw)};

    Vector3 eye =
        (Vector3){playerPos.x, playerPos.y + 1.5f, playerPos.z}; // eye height
    camera.position = eye;
    camera.target = Vector3Add(eye, forward);

    RenderSystem(&gs, camera);
  }

  CloseWindow();
  return 0;
}
