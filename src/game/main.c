#include "../engine/ecs/archetype.h"
#include "../engine/ecs/component.h"
#include "../engine/ecs/entity.h"
#include "../engine/ecs/world.h"
#include "../engine/util/bitset.h"
#include "gamestate.h"
#include "raylib.h"
#include <stdlib.h>

static Matrix TransformToMatrix(Transform t) {
  Matrix s = MatrixScale(t.scale.x, t.scale.y, t.scale.z);
  Matrix r = QuaternionToMatrix(t.rotation);
  Matrix p = MatrixTranslate(t.translation.x, t.translation.y, t.translation.z);
  return MatrixMultiply(MatrixMultiply(s, r), p);
}

int main(void) {
  /* ---------- raylib ---------- */

  InitWindow(1280, 720, "ECS + Raylib");
  SetTargetFPS(60);

  Camera3D camera = {0};
  camera.position = (Vector3){0, 2, 9};
  camera.target = (Vector3){0, 0, 0};
  camera.up = (Vector3){0, 1, 0};
  camera.fovy = 90.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  /* ---------- ECS ---------- */

  world_t *world = WorldCreate();

  componentPool_t modelCollectionPool;
  ComponentPoolInit(&modelCollectionPool, sizeof(ModelCollection_t));

  uint32_t bits[] = {COMP_TRANSFORM, COMP_MODEL_COLLECTION};
  bitset_t mask = MakeMask(bits, 2);

  archetype_t *arch = WorldCreateArchetype(world, &mask);
  ArchetypeAddInline(arch, COMP_TRANSFORM, sizeof(Transform));
  ArchetypeAddHandle(arch, COMP_MODEL_COLLECTION, &modelCollectionPool);

  /* ---------- Entity ---------- */

  entity_t e = WorldCreateEntity(world, &mask);

  Transform *t = WorldGetComponent(world, e, COMP_TRANSFORM);
  *t = TransformIdentity();
  t->translation = (Vector3){0, 0, 0};

  modelCollectionHandle_t handle = e.id;

  ModelCollection_t *mc =
      (ModelCollection_t *)ComponentAdd(&modelCollectionPool, handle);

  mc->count = 1;
  mc->modelInstances = calloc(1, sizeof(ModelInstance));

  mc->modelInstances[0].model = LoadModelFromMesh(GenMeshCube(1, 1, 1));
  mc->modelInstances[0].local = TransformIdentity();

  modelCollectionHandle_t *h =
      WorldGetComponent(world, e, COMP_MODEL_COLLECTION);
  *h = handle;

  /* ---------- Loop ---------- */

  while (!WindowShouldClose()) {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    BeginMode3D(camera);

    DrawGrid(20, 1.0f);

    // render entity
    Transform worldT = *t;
    Matrix worldMat = TransformToMatrix(worldT);

    for (uint32_t i = 0; i < mc->count; ++i) {
      ModelInstance *mi = &mc->modelInstances[i];

      Vector3 pos = worldT.translation;

      // Raylib uses axis + angle, not quaternion
      Vector3 axis;
      float angle;
      QuaternionToAxisAngle(worldT.rotation, &axis, &angle);
      angle *= RAD2DEG;

      Vector3 scale = worldT.scale;

      DrawModelEx(mi->model, pos, axis, angle, scale, WHITE);
      DrawCube((Vector3){0, 0.5f, 0}, 1, 1, 1, RED);
      DrawCubeWires((Vector3){0, 0.5f, 0}, 1, 1, 1, BLACK);
    }

    EndMode3D();

    DrawText("ECS + ModelCollection", 20, 20, 20, DARKGRAY);
    EndDrawing();
  }

  /* ---------- Cleanup ---------- */

  UnloadModel(mc->modelInstances[0].model);
  free(mc->modelInstances);
  ComponentRemove(&modelCollectionPool, handle);

  WorldDestroy(world);
  CloseWindow();

  return 0;
}
