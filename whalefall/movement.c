
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
} SoundType_t;

typedef struct {
  Sound sound;
} SoundAsset_t;

typedef struct {
  SoundType_t type;
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
  float *stepCycle;     // keeps track where the player is in the step cycle
  float *prevStepCycle; // keeps track of the previous step cycle, used for
                        // events on leg stomp
  float *stepRate;      // steps per second

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
  gs.entities.prevStepCycle = (float *)MemAlloc(sizeof(float) * MAX_ENTITIES);
  gs.entities.stepRate = (float *)MemAlloc(sizeof(float) * MAX_ENTITIES);

  // Init player
  gs.playerId = 0;
  gs.entities.positions[0] = (Vector3){0, 1, 0};
  gs.entities.velocities[0] = (Vector3){0, 0, 0};
  gs.entities.legYaw[0] = 0.0l;
  gs.entities.torsoYaw[0] = 0.0l;
  gs.entities.torsoPitch[0] = 0.0f;
  gs.entities.stepCycle[0] = 0.0f;
  gs.entities.prevStepCycle[0] = 0.0f;
  gs.entities.stepRate[0] = 2.0f;
  gs.pHeadbobTimer = 0;

  return gs;
}

SoundSystem_t InitSoundSystem() {
  SoundSystem_t sys = {0};
  InitAudioDevice();

  sys.assets[SOUND_FOOTSTEP].sound = LoadSound("assets/audio/mech_step_1.mp3");
  sys.assets[SOUND_WEAPON_FIRE].sound =
      LoadSound("assets/audio/cannon_shot_1.mp3");
  // sys.assets[SOUND_EXPLOSION].sound = LoadSound("assets/sfx/explosion.wav");
  // sys.assets[SOUND_UI_CLICK].sound = LoadSound("assets/sfx/ui_click.wav");

  return sys;
}

void QueueSound(SoundSystem_t *sys, SoundType_t type, Vector3 pos, float vol,
                float pitch) {
  if (sys->eventCount < MAX_SOUND_EVENTS) {
    sys->events[sys->eventCount++] = (SoundEvent_t){type, pos, vol, pitch};
  }
}

// ==================== Systems ====================

void PlayerControlSystem(GameState_t *gs, SoundSystem_t *soundSys, float dt) {
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

  // clamp pitch between -89° and +89°
  if (torsoPitch[pid] > 1.55f)
    torsoPitch[pid] = 1.55f; // ~89°
  if (torsoPitch[pid] < -1.55f)
    torsoPitch[pid] = -1.55f; // ~-89°

  // movement is still based on leg direction (not torso)
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

  // step cycle update
  Vector3 velocity = vel[pid];
  float speed = sqrtf(velocity.x * velocity.x + velocity.z * velocity.z);

  if (speed > 1.0f) {
    gs->pHeadbobTimer += dt * 8.0f;
    gs->entities.stepRate[pid] = speed * 0.25f;

    float prev = gs->entities.prevStepCycle[pid];
    float curr = gs->entities.stepCycle[pid] + gs->entities.stepRate[pid] * dt;

    // wrap cycle to [0,1)
    if (curr >= 1.0f)
      curr -= 1.0f;

    // detect stomp at 0.0 (wrap) and 0.5
    if (prev < 0.5f && curr >= 0.5f) {
      QueueSound(soundSys, SOUND_FOOTSTEP, pos[pid], 1.0f, 1.0f);
    }
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

void ProcessSoundSystem(SoundSystem_t *soundSys, Vector3 listenerPos) {
  for (int i = 0; i < soundSys->eventCount; i++) {
    SoundEvent_t event = soundSys->events[i];
    Sound sound = soundSys->assets[event.type].sound;

    // distance volume fade
    float dist = Vector3Distance(listenerPos, event.position);
    float atten = 1.0f / (1.0f + 0.1f * dist); // sound falloff

    SetSoundVolume(sound, event.volume * atten);
    SetSoundPitch(sound, event.pitch);
    PlaySound(sound);
  }
  soundSys->eventCount = 0; // clear for next frame
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
  SoundSystem_t soundSys = InitSoundSystem();

  float bobAmount = 0.2f; // height in meters, visual only

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    PlayerControlSystem(&gs, &soundSys, dt);
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
    ProcessSoundSystem(&soundSys, playerPos);
  }

  CloseWindow();
  return 0;
}
