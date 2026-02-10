#include "../engine/ecs/archetype.h"
#include "../engine/ecs/component.h"
#include "../engine/ecs/world.h"
#include "../engine/util/bitset.h"

#include "raylib.h"
#include "raymath.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define ECS_GET(world, entity, Type, ID)                                       \
  ((Type *)WorldGetComponent(world, entity, ID))

#define OMP_MIN_ITERATIONS 1024

/* ================= Components ================= */

enum {
  COMP_POSITION = 0,
  COMP_VELOCITY,
  COMP_ORIENTATION,
  COMP_MODEL,
  COMP_TIMER,
};

typedef enum {
  MODEL_ROT_WORLD,     // ignore entity orientation
  MODEL_ROT_YAW_ONLY,  // yaw only
  MODEL_ROT_YAW_PITCH, // yaw + pitch
  MODEL_ROT_FULL,      // future-proof (roll etc)
} ModelRotationMode;

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
  Vector3 offset;   // local position offset
  Vector3 rotation; // local rotation (Euler, radians)
  Vector3 scale;    // local scale
  ModelRotationMode rotationMode;
} ModelInstance_t;

typedef struct {
  uint32_t count;
  uint32_t capacity;
  ModelInstance_t *models;
} ModelCollection_t;

/* ================= Utilities ================= */

static bitset_t MakeMask(uint32_t *bits, uint32_t count) {
  bitset_t mask;
  BitsetInit(&mask, 64);
  for (uint32_t i = 0; i < count; ++i) {
    BitsetSet(&mask, bits[i]);
  }
  return mask;
}

void ModelCollectionInit(ModelCollection_t *mc, uint32_t initialCapacity) {
  mc->count = 0;
  mc->capacity = initialCapacity;
  mc->models = malloc(sizeof(ModelInstance_t) * initialCapacity);
}

void ModelCollectionAdd(ModelCollection_t *mc, ModelInstance_t instance) {
  if (mc->count >= mc->capacity) {
    mc->capacity = mc->capacity ? mc->capacity * 2 : 2;
    mc->models = realloc(mc->models, sizeof(ModelInstance_t) * mc->capacity);
  }
  mc->models[mc->count++] = instance;
}

void ModelCollectionFree(ModelCollection_t *mc) {
  free(mc->models);
  mc->models = NULL;
  mc->count = mc->capacity = 0;
}

static Vector3 ResolveModelRotation(const Orientation *entityOri,
                                    const ModelInstance_t *mi) {
  Vector3 result = mi->rotation;

  switch (mi->rotationMode) {
  case MODEL_ROT_WORLD:
    // no entity rotation
    break;

  case MODEL_ROT_YAW_ONLY:
    result.y += entityOri->yaw;
    break;

  case MODEL_ROT_YAW_PITCH:
    result.y += entityOri->yaw;
    result.x += entityOri->pitch;
    break;

  case MODEL_ROT_FULL:
    result.x += entityOri->pitch;
    result.y += entityOri->yaw;
    break;
  }

  return result;
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

  ori->pitch = Clamp(ori->pitch, -PI / 2 + 0.001, PI / 2 - 0.001);

  Vector3 forward = {
      cosf(ori->pitch) * sinf(ori->yaw),
      // sinf(ori->pitch),
      0,
      cosf(ori->pitch) * cosf(ori->yaw),
  };
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
#pragma omp parallel for if (arch->count >= OMP_MIN_ITERATIONS)
  for (uint32_t i = 0; i < arch->count; ++i) {
    entity_t e = arch->entities[i];

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    Velocity *vel = ECS_GET(world, e, Velocity, COMP_VELOCITY);

    pos->value = Vector3Add(pos->value, Vector3Scale(vel->value, dt));
  }
}

void TimerSystem(componentPool_t *timerPool, float dt) {
  Timer *timers = (Timer *)timerPool->denseData;
  for (uint32_t i = 0; i < timerPool->count; ++i) {
    timers[i].value -= dt;
    timers[i].value = timers[i].value <= 0 ? 0 : timers[i].value;
  }
}

void UpdateCubesSystem(world_t *world, archetype_t *arch, float dt) {
#pragma omp parallel for if (arch->count >= OMP_MIN_ITERATIONS)
  for (uint32_t i = 0; i < arch->count; ++i) {
    entity_t e = arch->entities[i];

    Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);

    ori->yaw += 1 * dt;
  }
}

void RenderSystem(world_t *world, archetype_t *arch) {
#pragma omp parallel for if (arch->count >= OMP_MIN_ITERATIONS)
  for (uint32_t i = 0; i < arch->count; ++i) {
    entity_t e = arch->entities[i];

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
    ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

    for (uint32_t m = 0; m < mc->count; ++m) {
      ModelInstance_t *mi = &mc->models[m];

      Vector3 worldPos = Vector3Add(pos->value, mi->offset);
      Vector3 rot = ResolveModelRotation(ori, mi);

      Matrix transform =
          MatrixMultiply(MatrixRotateY(rot.y), MatrixRotateX(rot.x));

      DrawModelEx(mi->model, worldPos, (Vector3){0, 1, 0}, rot.y * RAD2DEG,
                  mi->scale, WHITE);
    }
  }
}

/* ================= Main ================= */

int main(void) {

  SetConfigFlags(FLAG_VSYNC_HINT);
  InitWindow(1280, 720, "ECS FPS Test");
  DisableCursor();
  SetTargetFPS(0);

  Camera3D camera = {0};
  camera.fovy = 75.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  world_t *world = WorldCreate();

  /* ---------- Model Pool ---------- */

  componentPool_t modelPool;
  ComponentPoolInit(&modelPool, sizeof(ModelCollection_t));

  componentPool_t timerPool;
  ComponentPoolInit(&timerPool, sizeof(Timer));

  Model cube = LoadModelFromMesh(GenMeshCube(1, 1, 1));

  /* ---------- Player Archetype ---------- */

  uint32_t playerBits[] = {COMP_POSITION, COMP_VELOCITY, COMP_ORIENTATION,
                           COMP_MODEL, COMP_TIMER};
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

  ModelCollection_t *mc = ECS_GET(world, player, ModelCollection_t, COMP_MODEL);
  ModelCollectionInit(mc, 2);

  ModelCollectionAdd(mc, (ModelInstance_t){.model = cube,
                                           .offset = (Vector3){0, -2, 0},
                                           .rotation = (Vector3){0, 0, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_YAW_ONLY});

  Model gun = LoadModel("assets/models/gun1.glb");
  ModelCollectionAdd(mc, (ModelInstance_t){.model = gun,
                                           .offset = (Vector3){0, -.9f, 0},
                                           .rotation = (Vector3){0, 0, 0},
                                           .scale = (Vector3){1, 1, 1},
                                           .rotationMode = MODEL_ROT_FULL});

  ECS_GET(world, player, Timer, COMP_TIMER)->value = 5.0f;

  /* ---------- Box Archetype ---------- */

  uint32_t boxBits[] = {COMP_POSITION, COMP_ORIENTATION, COMP_MODEL};
  bitset_t boxMask = MakeMask(boxBits, 3);

  archetype_t *boxArch = WorldCreateArchetype(world, &boxMask);
  ArchetypeAddInline(boxArch, COMP_POSITION, sizeof(Position));
  ArchetypeAddInline(boxArch, COMP_ORIENTATION, sizeof(Orientation));
  ArchetypeAddHandle(boxArch, COMP_MODEL, &modelPool);

  for (int i = 0; i < 100; ++i) {
    entity_t box = WorldCreateEntity(world, &boxMask);

    ECS_GET(world, box, Position, COMP_POSITION)->value =
        (Vector3){i * 2.0f, 0.5f, 5.0f};

    ModelCollection_t *mc = ECS_GET(world, box, ModelCollection_t, COMP_MODEL);

    ModelCollectionInit(mc, 1);

    ModelCollectionAdd(mc, (ModelInstance_t){
                               .model = cube,
                               .offset = (Vector3){0, 0, 0},
                               .rotation = (Vector3){0, 0, 0},
                               .scale = (Vector3){1, 1, 1},
                           });
  }

  /* ---------- Main Loop ---------- */

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();

    TimerSystem(&timerPool, dt);

    // printf("player timer:%f \n",
    //        ECS_GET(world, player, Timer, COMP_TIMER)->value);

    PlayerControlSystem(world, player);
    MovementSystem(world, playerArch, dt);
    UpdateCubesSystem(world, boxArch, dt);

    Orientation *ori = ECS_GET(world, player, Orientation, COMP_ORIENTATION);
    Position *pos = ECS_GET(world, player, Position, COMP_POSITION);

    camera.position = pos->value;

    Vector3 forward = {
        cosf(ori->pitch) * sinf(ori->yaw),
        sinf(ori->pitch),
        cosf(ori->pitch) * cosf(ori->yaw),
    };
    camera.target = Vector3Add(pos->value, forward);
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
