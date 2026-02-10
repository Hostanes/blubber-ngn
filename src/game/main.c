#include "../engine/ecs/archetype.h"
#include "../engine/ecs/component.h"
#include "../engine/ecs/world.h"
#include "../engine/util/bitset.h"

#include "raylib.h"
#include "raymath.h"

#include <math.h>
#include <stdio.h>

#define ECS_GET(world, entity, Type, ID)                                       \
  ((Type *)WorldGetComponent(world, entity, ID))

/* ================= Components ================= */

enum {
  COMP_POSITION = 0,
  COMP_VELOCITY,
  COMP_ORIENTATION,
  COMP_MODEL,
  COMP_TIMER,
};

typedef struct {
  Vector3 value;
} Position;

typedef struct {
  Vector3 value;
} Velocity;

typedef struct {
  float value;
} Timer;

typedef struct {
  float yaw;
  float pitch;
} Orientation;

typedef struct {
  Model model;
} ModelComponent;

/* ================= Utilities ================= */

static bitset_t MakeMask(uint32_t *bits, uint32_t count) {
  bitset_t mask;
  BitsetInit(&mask, 64);
  for (uint32_t i = 0; i < count; ++i) {
    BitsetSet(&mask, bits[i]);
  }
  return mask;
}

/* ================= Systems ================= */

void PlayerControlSystem(world_t *world, entity_t player) {
  Position *pos = ECS_GET(world, player, Position, COMP_POSITION);
  Velocity *vel = ECS_GET(world, player, Velocity, COMP_VELOCITY);
  Orientation *ori = ECS_GET(world, player, Orientation, COMP_ORIENTATION);

  (void)pos; // currently unused

  const float speed = 5.0f;
  const float mouseSensitivity = 0.002f;

  Vector2 mouse = GetMouseDelta();
  ori->yaw -= mouse.x * mouseSensitivity;
  ori->pitch -= mouse.y * mouseSensitivity;

  ori->pitch = Clamp(ori->pitch, -PI / 2, PI / 2);

  Vector3 forward = {sinf(ori->yaw), 0.0f, cosf(ori->yaw)};
  Vector3 right = {cosf(ori->yaw), 0.0f, -sinf(ori->yaw)};

  vel->value = (Vector3){0};

  if (IsKeyDown(KEY_W))
    vel->value = Vector3Add(vel->value, forward);
  if (IsKeyDown(KEY_S))
    vel->value = Vector3Subtract(vel->value, forward);
  if (IsKeyDown(KEY_A))
    vel->value = Vector3Add(vel->value, right);
  if (IsKeyDown(KEY_D))
    vel->value = Vector3Subtract(vel->value, right);

  if (Vector3Length(vel->value) > 0.0f) {
    vel->value = Vector3Scale(Vector3Normalize(vel->value), speed);
  }
}

void MovementSystem(world_t *world, archetype_t *arch, float dt) {
  for (uint32_t i = 0; i < arch->count; ++i) {
    entity_t e = arch->entities[i];

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    Velocity *vel = ECS_GET(world, e, Velocity, COMP_VELOCITY);

    pos->value = Vector3Add(pos->value, Vector3Scale(vel->value, dt));
  }
}

void timerSystem(componentPool_t *timerPool, float dt) {
  Timer *timers = (Timer *)timerPool->denseData;
  for (uint32_t i = 0; i < timerPool->count; ++i) {
    timers[i].value -= dt;
    timers[i].value = timers[i].value <= 0 ? 0 : timers[i].value;
  }
}

void RenderSystem(world_t *world, archetype_t *arch) {
  for (uint32_t i = 0; i < arch->count; ++i) {
    entity_t e = arch->entities[i];

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
    ModelComponent *mc = ECS_GET(world, e, ModelComponent, COMP_MODEL);

    DrawModelEx(mc->model, pos->value, (Vector3){0, 1, 0}, ori->yaw * RAD2DEG,
                (Vector3){1, 1, 1}, WHITE);
  }
}

/* ================= Main ================= */

int main(void) {
  InitWindow(1280, 720, "ECS FPS Test");
  DisableCursor();
  SetTargetFPS(60);

  Camera3D camera = {0};
  camera.fovy = 75.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  world_t *world = WorldCreate();

  /* ---------- Model Pool ---------- */

  componentPool_t modelPool;
  ComponentPoolInit(&modelPool, sizeof(ModelComponent));

  componentPool_t timerPool;
  ComponentPoolInit(&timerPool, sizeof(float));

  Model cube = LoadModelFromMesh(GenMeshCube(1, 1, 1));

  /* ---------- Player Archetype ---------- */

  uint32_t playerBits[] = {COMP_POSITION, COMP_VELOCITY, COMP_ORIENTATION,
                           COMP_MODEL | COMP_TIMER};
  bitset_t playerMask = MakeMask(playerBits, 5);

  archetype_t *playerArch = WorldCreateArchetype(world, &playerMask);
  ArchetypeAddInline(playerArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(playerArch, COMP_VELOCITY, sizeof(Velocity));
  ArchetypeAddInline(playerArch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddHandle(playerArch, COMP_MODEL, &modelPool);
  ArchetypeAddHandle(playerArch, COMP_TIMER, &timerPool);

  entity_t player = WorldCreateEntity(world, &playerMask);

  ECS_GET(world, player, Position, COMP_POSITION)->value =
      (Vector3){0, 1.8f, 0};

  ECS_GET(world, player, ModelComponent, COMP_MODEL)->model = cube;

  ECS_GET(world, player, Timer, COMP_TIMER)->value = 5.0f;

  /* ---------- Box Archetype ---------- */

  uint32_t boxBits[] = {COMP_POSITION, COMP_ORIENTATION, COMP_MODEL};
  bitset_t boxMask = MakeMask(boxBits, 3);

  archetype_t *boxArch = WorldCreateArchetype(world, &boxMask);
  ArchetypeAddInline(boxArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(boxArch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddHandle(boxArch, COMP_MODEL, &modelPool);

  for (int i = 0; i < 10; ++i) {
    entity_t box = WorldCreateEntity(world, &boxMask);

    ECS_GET(world, box, Position, COMP_POSITION)->value =
        (Vector3){i * 2.0f, 0.5f, 5.0f};

    ECS_GET(world, box, ModelComponent, COMP_MODEL)->model = cube;
  }

  /* ---------- Main Loop ---------- */

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    timerSystem(&timerPool, dt);

    printf("player timer:%f \n",
           ECS_GET(world, player, Timer, COMP_TIMER)->value);

    PlayerControlSystem(world, player);
    MovementSystem(world, playerArch, dt);

    Orientation *ori = ECS_GET(world, player, Orientation, COMP_ORIENTATION);
    Position *pos = ECS_GET(world, player, Position, COMP_POSITION);

    camera.position = pos->value;
    camera.target =
        Vector3Add(pos->value,
                   (Vector3){sinf(ori->yaw), sinf(ori->pitch), cosf(ori->yaw)});
    camera.up = (Vector3){0, 1, 0};

    BeginDrawing();
    ClearBackground(SKYBLUE);
    BeginMode3D(camera);

    RenderSystem(world, boxArch);
    RenderSystem(world, playerArch);

    EndMode3D();
    DrawFPS(10, 10);
    EndDrawing();
  }

  CloseWindow();
  return 0;
}
