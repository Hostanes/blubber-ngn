#include "../game.h"
#include "systems.h"
#include <raylib.h>

static bitset_t modelMask;
static bool modelMaskInit = false;

static void EnsureModelMask(void) {
  if (modelMaskInit)
    return;

  BitsetInit(&modelMask, 64);
  BitsetSet(&modelMask, COMP_MODEL);

  modelMaskInit = true;
}

void RenderMainMenu(GameWorld *game) {
  BeginDrawing();
  ClearBackground(DARKGRAY);

  const char *title = "FPS test game";
  const char *prompt = "Press ENTER to start";

  int screenW = GetScreenWidth();
  int screenH = GetScreenHeight();

  DrawText(title, screenW / 2 - MeasureText(title, 40) / 2, screenH / 2 - 60,
           40, RAYWHITE);

  DrawText(prompt, screenW / 2 - MeasureText(prompt, 20) / 2, screenH / 2 + 10,
           20, LIGHTGRAY);

  EndDrawing();
}

void RenderArchetype(world_t *world, archetype_t *arch) {

#pragma omp parallel for if (arch->count >= OMP_MIN_ITERATIONS)
  for (uint32_t i = 0; i < arch->count; ++i) {
    entity_t e = arch->entities[i];

    Position *pos = ECS_GET(world, e, Position, COMP_POSITION);
    Orientation *ori = ECS_GET(world, e, Orientation, COMP_ORIENTATION);
    ModelCollection_t *mc = ECS_GET(world, e, ModelCollection_t, COMP_MODEL);

    // Entity yaw (world-space)
    Matrix T_world = MatrixTranslate(pos->value.x, pos->value.y, pos->value.z);
    Matrix R_entityYaw = MatrixRotateY(ori->yaw);
    Matrix entityTransform = MatrixMultiply(R_entityYaw, T_world);

    for (uint32_t m = 0; m < mc->count; ++m) {
      ModelInstance_t *mi = &mc->models[m];

      // Local model transform
      Matrix local =
          MatrixMultiply(MatrixScale(mi->scale.x, mi->scale.y, mi->scale.z),
                         MatrixRotateXYZ(mi->rotation));

      local = MatrixMultiply(
          MatrixTranslate(mi->offset.x, mi->offset.y, mi->offset.z), local);

      // Final transform
      Matrix transform = MatrixMultiply(local, entityTransform);

      mi->model.transform = transform;
      DrawModel(mi->model, (Vector3){0, 0, 0}, 1.0f, WHITE);
    }
  }
}

void RenderLevelSystem(world_t *world, GameWorld *game, Camera *camera) {
  EnsureModelMask();

  BeginDrawing();
  ClearBackground(GRAY);
  BeginMode3D(*camera);

  DrawGrid(50, 5.0f);

  for (uint32_t i = 0; i < world->archetypeCount; ++i) {
    archetype_t *arch = &world->archetypes[i];

    if (!BitsetContainsAll(&arch->mask, &modelMask))
      continue;

    RenderArchetype(world, arch);
  }

  EndMode3D();
  DrawFPS(10, 10);
  EndDrawing();
}
