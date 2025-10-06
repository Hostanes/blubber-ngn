
// systems.c
// Implements player input, physics, and rendering

#include "systems.h"
#include "raymath.h"

#include "raylib.h"

Model mechLeg;

void LoadAssets() { mechLeg = LoadModel("assets/models/raptor1-legs.glb"); }

void UnloadAssets() { UnloadModel(mechLeg); }

// ---------------- Player Control ----------------

void PlayerControlSystem(GameState_t *gs, SoundSystem_t *soundSys, float dt) {
  int pid = gs->playerId;
  Vector3 *pos = gs->entities.positions;
  Vector3 *vel = gs->entities.velocities;
  Orientation *leg = gs->entities.legOrientation;
  Orientation *torso = gs->entities.torsoOrientation;

  // Rotate legs with A/D
  if (IsKeyDown(KEY_A))
    leg[pid].yaw -= 1.5f * dt;
  if (IsKeyDown(KEY_D))
    leg[pid].yaw += 1.5f * dt;

  // Torso yaw/pitch from mouse
  Vector2 mouse = GetMouseDelta();
  float sensitivity = 0.001f;
  torso[pid].yaw += mouse.x * sensitivity;
  torso[pid].pitch += -mouse.y * sensitivity;

  // Clamp torso pitch between -89° and +89°
  if (torso[pid].pitch > 1.2f)
    torso[pid].pitch = 1.2f;
  if (torso[pid].pitch < -1.0f)
    torso[pid].pitch = -1.0f;

  // Movement is based on leg orientation
  float c = cosf(leg[pid].yaw);
  float s = sinf(leg[pid].yaw);
  Vector3 forward = {c, 0, s};
  Vector3 right = {-s, 0, c};

  // Movement keys
  if (IsKeyDown(KEY_W)) {
    vel[pid].x += forward.x * 100.0f * dt;
    vel[pid].z += forward.z * 100.0f * dt;
  }
  if (IsKeyDown(KEY_S)) {
    vel[pid].x -= forward.x * 40.0f * dt;
    vel[pid].z -= forward.z * 40.0f * dt;
  }
  if (IsKeyDown(KEY_Q)) {
    vel[pid].x += right.x * -40.0f * dt;
    vel[pid].z += right.z * -40.0f * dt;
  }
  if (IsKeyDown(KEY_E)) {
    vel[pid].x += right.x * 40.0f * dt;
    vel[pid].z += right.z * 40.0f * dt;
  }

  // Step cycle update
  Vector3 velocity = vel[pid];
  float speed = sqrtf(velocity.x * velocity.x + velocity.z * velocity.z);

  if (speed > 1.0f) {
    gs->pHeadbobTimer += dt * 8.0f;
    gs->entities.stepRate[pid] = speed * 0.25f;

    float prev = gs->entities.prevStepCycle[pid];
    float curr = gs->entities.stepCycle[pid] + gs->entities.stepRate[pid] * dt;

    if (curr >= 1.0f)
      curr -= 1.0f; // wrap cycle

    if (prev > curr) { // wrapped around -> stomp
      QueueSound(soundSys, SOUND_FOOTSTEP, pos[pid], 1.0f, 1.0f);
    }

    gs->entities.stepCycle[pid] = curr;
    gs->entities.prevStepCycle[pid] = curr;
  } else {
    gs->pHeadbobTimer = 0;
    gs->entities.stepCycle[pid] = 0.0f;
    gs->entities.prevStepCycle[pid] = 0.0f;
  }
}

// ---------------- Physics ----------------

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

// ---------------- Rendering ----------------

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
  float yaw = gs->entities.legOrientation[pid].yaw;
  Vector3 forward = {cosf(yaw), 0, sinf(yaw)};

  // Cylinder
  // Vector3 base = {pos.x, pos.y + 0.8f, pos.z};
  // Vector3 shaftEnd = {base.x + forward.x * 0.95f, base.y,
  //                     base.z + forward.z * 0.95f};
  // DrawCylinderEx(base, shaftEnd, 0.1f, 0.1f, 8, BLUE);

  // Vector3 headEnd = {shaftEnd.x + forward.x * 0.55f, shaftEnd.y,
  //                    shaftEnd.z + forward.z * 0.55f};
  // DrawCylinderEx(shaftEnd, headEnd, 0.25f, 0.0f, 8, RED);

  Matrix legTransform = MatrixRotateY(gs->entities.legOrientation[pid].yaw);
  legTransform = MatrixMultiply(
      MatrixRotateX(gs->entities.legOrientation[pid].pitch), legTransform);
  legTransform = MatrixMultiply(
      MatrixRotateZ(gs->entities.legOrientation[pid].roll), legTransform);

  // Translate to player position
  legTransform =
      MatrixMultiply(MatrixTranslate(pos.x, pos.y, pos.z), legTransform);

  DrawModelEx(mechLeg, pos, (Vector3){0, 1, 0},
              gs->entities.legOrientation[pid].yaw * RAD2DEG * -1,
              (Vector3){1, 1, 1}, YELLOW);
  DrawModelWiresEx(mechLeg, pos, (Vector3){0, 1, 0},
              gs->entities.legOrientation[pid].yaw * RAD2DEG * -1,
              (Vector3){1, 1, 1}, RED);

  DrawCircle3D(pos, 3.0f, (Vector3){1, 0, 0}, 90.0f, (Color){0, 0, 0, 100});

  EndMode3D();

  DrawFPS(10, 10);
  EndDrawing();
}
