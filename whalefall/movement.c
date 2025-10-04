
#include "raylib.h"
#include "raymath.h"
#include "rcamera.h"
#include <math.h>

#define MAX_ENTITIES 1

#define MAX_SOUNDS 128
#define MAX_SOUND_EVENTS 256

typedef enum {
  SOUND_FOOTSTEP,
  SOUND_WEAPON_FIRE,
  SOUND_EXPLOSION,
  SOUND_UI_CLICK,
  SOUND_COUNT
} SoundType_T;

typedef struct {
  Sound sound;
} SoundAsset_t;

typedef struct {
  SoundType_T type;
  Vector3 position;
  float volume;
  float pitch;
} SoundEvent_t;

typedef struct {
  SoundAsset_t assets[MAX_SOUNDS];
  SoundEvent_t events[MAX_SOUND_EVENTS];
  int eventCount;
} SoundSystem_t;

// Stores data for all entities
typedef struct EntityData {
  int count;
  Vector3 *positions;
  Vector3 *velocities;

  // TODO raptor only, when adding other types of enemies gotta figure out what
  // to do with this
  float *legYaw;
  float *torsoYaw;
  float *torsoPitch;
  float *stepCycle; // keeps track where the player is in the step cycle
  float *stepRate;  // steps per second

} EntityData_t;

typedef struct GameState {
  EntityData_t entities;
  int playerId; // player isnt a unique struct, just entity[0]
  float pHeadbobTimer;

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
  gs.entities.stepCycle = (float *)MemAlloc(sizeof(float) * MAX_ENTITIES);
  gs.entities.stepRate = (float *)MemAlloc(sizeof(float) * MAX_ENTITIES);

  // Init player
  gs.playerId = 0;
  gs.entities.positions[0] = (Vector3){0, 1, 0};
  gs.entities.velocities[0] = (Vector3){0, 0, 0};
  gs.entities.legYaw[0] = 0.0l;
  gs.entities.torsoYaw[0] = 0.0l;
  gs.entities.torsoPitch[0] = 0.0f;
  gs.entities.stepCycle[0] = 0.0f;
  gs.entities.stepRate[0] = 2.0f;
  gs.pHeadbobTimer = 0;

  return gs;
}

SoundSystem_t InitSoundSystem() {
  SoundSystem_t sys = {0};
  InitAudioDevice();

  // sys.assets[SOUND_FOOTSTEP].sound = LoadSound("assets/sfx/footstep.wav");
  // sys.assets[SOUND_WEAPON_FIRE].sound = LoadSound("assets/sfx/laser.wav");
  // sys.assets[SOUND_EXPLOSION].sound = LoadSound("assets/sfx/explosion.wav");
  // sys.assets[SOUND_UI_CLICK].sound = LoadSound("assets/sfx/ui_click.wav");

  return sys;
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
  Vector3 velocity = vel[pid];
  float speed = sqrtf(velocity.x * velocity.x + velocity.z * velocity.z);
  if (speed > 1.0f) {
    gs->pHeadbobTimer += dt * 8.0f;
    gs->entities.stepRate[0] = speed * 0.25f;
    gs->entities.stepCycle[0] += gs->entities.stepRate[0] * dt;

    // keep it in [0,1)
    if (gs->entities.stepCycle[0] >= 1.0f)
      gs->entities.stepCycle[0] -= 1.0f;
  } else {
    gs->pHeadbobTimer = 0;
    gs->entities.stepCycle[0] = 0.0f;
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

  float bobAmount = 0.2f; // height in meters, visual only

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

    // bob ammount calculation, skewed sine wave

    // Or sharper descent (triangle wave)
    float t = gs.entities.stepCycle[0];
    float bobTri = (t < 0.5f) ? (t * 2) : (2 - t * 2); // 0→1→0
    bobTri = 1.0f - bobTri;                            // flip to make "drop"
    float bobY = bobTri * bobAmount;

    //

    Vector3 eye = (Vector3){playerPos.x, playerPos.y + 1.5f + bobY,
                            playerPos.z}; // eye height
    camera.position = eye;
    camera.target = Vector3Add(eye, forward);

    RenderSystem(&gs, camera);
  }

  CloseWindow();
  return 0;
}
